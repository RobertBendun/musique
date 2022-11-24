#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <thread>
#include <cstring>
#include <cstdio>
#include <mutex>

#include <musique/format.hh>
#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/lexer/lines.hh>
#include <musique/midi/midi.hh>
#include <musique/parser/parser.hh>
#include <musique/pretty.hh>
#include <musique/try.hh>
#include <musique/unicode.hh>
#include <musique/value/block.hh>

#ifdef _WIN32
extern "C" {
#include <io.h>
}
#else
#include <unistd.h>
extern "C" {
#include <bestline.h>
}
#endif

namespace fs = std::filesystem;

static bool quiet_mode = false;
static bool ast_only_mode = false;
static bool enable_repl = false;
static unsigned repl_line_number = 1;

std::mutex stdio_mutex;

#define Ignore(Call) do { auto const ignore_ ## __LINE__ = (Call); (void) ignore_ ## __LINE__; } while(0)

/// Pop string from front of an array
template<typename T = std::string_view>
static T pop(std::span<char const*> &span)
{
	auto element = span.front();
	span = span.subspan(1);

	if constexpr (std::is_same_v<T, std::string_view>) {
		return element;
	} else if constexpr (std::is_arithmetic_v<T>) {
		T result;
		auto end = element + std::strlen(element);
		auto [ptr, ec] = std::from_chars(element, end, result);
		if (ec != decltype(ec){}) {
			std::cout << "Expected natural number as argument" << std::endl;
			std::exit(1);
		}
		return result;
	} else {
		static_assert(always_false<T>, "Unsupported type for pop operation");
	}
}

/// Print usage and exit
[[noreturn]] void usage()
{
	std::cerr <<
		"usage: musique <options> [filename]\n"
		"  where filename is path to file with Musique code that will be executed\n"
		"  where options are:\n"
		"    -o,--output PORT\n"
		"      provides output port, a place where Musique produces MIDI messages\n"
		"    -l,--list\n"
		"      lists all available MIDI ports and quit\n"
		"\n"
		"    -c,--run CODE\n"
		"      executes given code\n"
		"    -I,--interactive,--repl\n"
		"      enables interactive mode even when another code was passed\n"
		"\n"
		"    -f,--as-function FILENAME\n"
		"      deffer file execution and turn it into a file\n"
		"\n"
		"    --ast\n"
		"      prints ast for given code\n"
		"\n"
		"Thanks to:\n"
		"  Sy Brand, https://sybrand.ink/, creator of tl::expected https://github.com/TartanLlama/expected\n"
		"  Justine Tunney, https://justinetunney.com, creator of bestline readline library https://github.com/jart/bestline\n"
		"  Gary P. Scavone, http://www.music.mcgill.ca/~gary/, creator of rtmidi https://github.com/thestk/rtmidi\n"
		;
	std::exit(1);
}

void print_repl_help()
{
	std::cout <<
		"List of all available commands of Musique interactive mode:\n"
		":exit - quit interactive mode\n"
		":help - prints this help message\n"
		":!<command> - allows for execution of any shell command\n"
		":clear - clears screen\n"
		":load <file> - loads file into Musique session\n"
		;
}

/// Trim spaces from left an right
static void trim(std::string_view &s)
{
	// left trim
	if (auto const i = std::find_if_not(s.begin(), s.end(), unicode::is_space); i != s.begin()) {
		// std::string_view::remove_prefix has UB when we wan't to remove more characters then str has
		// src: https://en.cppreference.com/w/cpp/string/basic_string_view/remove_prefix
		if (i != s.end()) {
			s.remove_prefix(std::distance(s.begin(), i));
		} else {
			s = {};
		}
	}

	// right trim
	if (auto const ws_end = std::find_if_not(s.rbegin(), s.rend(), unicode::is_space); ws_end != s.rbegin()) {
		s.remove_suffix(std::distance(ws_end.base(), s.end()));
	}
}

static std::string filename_to_function_name(std::string_view filename)
{
	if (filename == "-") {
		return "stdin";
	}

	auto stem = fs::path(filename).stem().string();
	auto stem_view = std::string_view(stem);

	std::string name;

	auto is_first = unicode::First_Character::Yes;
	while (!stem_view.empty()) {
		auto [rune, next_stem] = utf8::decode(stem_view);
		if (!unicode::is_identifier(rune, is_first)) {
			if (rune != ' ' && rune != '-') {
				// TODO Nicer error
				std::cerr << "[ERROR] Failed to construct function name from filename " << stem
					        << "\n       due to presence of invalid character: " << utf8::Print{rune} << "(U+" << std::hex << rune << ")\n"
									<< std::flush;
				std::exit(1);
			}
			name += '_';
		} else {
			name += stem_view.substr(0, stem_view.size() - next_stem.size());
		}
		stem_view = next_stem;
	}
	return name;
}

/// Runs interpreter on given source code
struct Runner
{
	static inline Runner *the;

	midi::Rt_Midi midi;
	Interpreter interpreter;

	/// Setup interpreter and midi connection with given port
	explicit Runner(std::optional<unsigned> output_port)
		: midi()
		, interpreter{}
	{
		ensure(the == nullptr, "Only one instance of runner is supported");
		the = this;

		interpreter.midi_connection = &midi;
		if (output_port) {
			midi.connect_output(*output_port);
			if (!quiet_mode) {
				std::cout << "Connected MIDI output to port " << *output_port << ". Ready to play!" << std::endl;
			}
		} else {
			bool connected_to_existing_port = midi.connect_or_create_output();
			if (!quiet_mode) {
				if (connected_to_existing_port) {
					std::cout << "Connected MIDI output to port " << midi.output->getPortName(0) << std::endl;
				} else {
					std::cout << "Created new MIDI output port 'Musique'. Ready to play!" << std::endl;
				}
			}
		}

		Env::global->force_define("print", +[](Interpreter &interpreter, std::vector<Value> args) -> Result<Value> {
			for (auto it = args.begin(); it != args.end(); ++it) {
				{
					auto result = Try(format(interpreter, *it));
					std::lock_guard guard{stdio_mutex};
					std::cout << result;
				}

				if (std::next(it) != args.end()) {
					std::lock_guard guard{stdio_mutex};
					std::cout << ' ';
				}
			}
			std::lock_guard guard{stdio_mutex};
			std::cout << std::endl;
			return {};
		});
	}

	Runner(Runner const&) = delete;
	Runner(Runner &&) = delete;
	Runner& operator=(Runner const&) = delete;
	Runner& operator=(Runner &&) = delete;

	/// Consider given input file as new definition of parametless function
	///
	/// Useful for deffering execution of files to the point when all configuration of midi devices
	/// is beeing known as working.
	std::optional<Error> deffered_file(std::string_view source, std::string_view filename)
	{
		auto ast = Try(Parser::parse(source, filename, repl_line_number));
		if (ast_only_mode) {
			dump(ast);
			return {};
		}

		auto name = filename_to_function_name(filename);

		Block block;
		block.location = ast.location;
		block.body = std::move(ast);
		block.context = Env::global;
		std::cout << "Defined function " << name << " as file " << filename << std::endl;
		Env::global->force_define(std::move(name), Value(std::move(block)));
		return {};
	}

	/// Run given source
	std::optional<Error> run(std::string_view source, std::string_view filename, bool output = false)
	{
		auto ast = Try(Parser::parse(source, filename, repl_line_number));

		if (ast_only_mode) {
			dump(ast);
			return {};
		}
		if (auto result = Try(interpreter.eval(std::move(ast))); output && not holds_alternative<Nil>(result)) {
			std::cout << Try(format(interpreter, result)) << std::endl;
		}
		return {};
	}
};

/// All source code through life of the program should stay allocated, since
/// some of the strings are only views into source
std::vector<std::string> eternal_sources;

#ifndef _WIN32
void completion(char const* buf, bestlineCompletions *lc)
{
	std::string_view in{buf};

	for (auto scope = Runner::the->interpreter.env.get(); scope != nullptr; scope = scope->parent.get()) {
		for (auto const& [name, _] : scope->variables) {
			if (name.starts_with(in)) {
				bestlineAddCompletion(lc, name.c_str());
			}
		}
	}
}
#endif

bool is_tty()
{
#ifdef _WIN32
	return _isatty(STDOUT_FILENO);
#else
	return isatty(fileno(stdout));
#endif
}

/// Handles commands inside REPL session (those starting with ':')
///
/// Returns if one of command matched
static Result<bool> handle_repl_session_commands(std::string_view input, Runner &runner)
{
	using Handler = std::optional<Error>(*)(Runner &runner, std::optional<std::string_view> arg);
	using Command = std::pair<std::string_view, Handler>;
	static constexpr auto Commands = std::array {
		Command { "exit",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::exit(0);
			}
		},
		Command { "quit",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::exit(0);
			}
		},
		Command { "clear",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::cout << "\x1b[1;1H\x1b[2J" << std::flush;
				return {};
			}
		},
		Command { "help",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				print_repl_help();
				return {};
			}
		},
		Command { "load",
			+[](Runner& runner, std::optional<std::string_view> arg) -> std::optional<Error> {
				if (not arg.has_value()) {
					std::cerr << ":load subcommand requires path to file that will be loaded" << std::endl;
					return {};
				}
				auto path = *arg;
				std::ifstream source_file{std::string(path)};
				if (not source_file.is_open()) {
					std::cerr << ":load cannot find file " << path << std::endl;
					return {};
				}
				eternal_sources.emplace_back(std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>());
				Lines::the.add_file(std::string(path), eternal_sources.back());
				return runner.run(eternal_sources.back(), path);
			}
		},
		Command {
			"snap",
			+[](Runner &runner, std::optional<std::string_view> arg) -> std::optional<Error> {
				std::ostream *out = &std::cout;
				std::fstream file;
				if (arg.has_value() && arg->size()) {
					file.open(std::string(*arg));
				}
				runner.interpreter.snapshot(*out);
				return std::nullopt;
			}
		},
	};

	if (input.starts_with('!')) {
		// TODO Maybe use different shell invoking mechanism then system
		// since on Windows it uses cmd.exe and not powershell which is
		// strictly superior
		Ignore(system(input.data() + 1));
		return true;
	}

	auto ws = input.find_first_of(" \t\n");
	auto provided_command = input.substr(0, input.find_first_of(" \t\n"));
	auto arg = ws == std::string_view::npos ? std::string_view{} : input.substr(ws);
	trim(arg);

	for (auto [command_name, handler] : Commands) {
		if (provided_command == command_name) {
			Try(handler(runner, arg));
			return true;
		}
	}

	return false;
}

/// Fancy main that supports Result forwarding on error (Try macro)
static std::optional<Error> Main(std::span<char const*> args)
{
	if (is_tty() && getenv("NO_COLOR") == nullptr) {
		pretty::terminal_mode();
	}

	/// Describes all arguments that will be run
	struct Run
	{
		enum Type
		{
			File,
			Argument,
			Deffered_File
		} type;
		std::string_view argument;
	};

	// Arbitraly chosen for conviniance of the author
	std::optional<unsigned> output_port{};

	std::vector<Run> runnables;

	while (not args.empty()) {
		std::string_view arg = pop(args);

		if (arg == "-" || !arg.starts_with('-')) {
			runnables.push_back({ .type = Run::File, .argument = std::move(arg) });
			continue;
		}

		if (arg == "-l" || arg == "--list") {
			midi::Rt_Midi{}.list_ports(std::cout);
			return {};
		}

		if (arg == "-c" || arg == "--run") {
			if (args.empty()) {
				std::cerr << "musique: error: option " << arg << " requires an argument" << std::endl;
				std::exit(1);
			}
			runnables.push_back({ .type = Run::Argument, .argument = pop(args) });
			continue;
		}

		if (arg == "-f" || arg == "--as-function") {
			if (args.empty()) {
				std::cerr << "musique: error: option " << arg << " requires an argument" << std::endl;
				std::exit(1);
			}
			runnables.push_back({ .type = Run::Deffered_File, .argument = pop(args) });
			continue;
		}

		if (arg == "--quiet" || arg == "-q") {
			quiet_mode = true;
			continue;
		}
		if (arg == "--ast") {
			ast_only_mode = true;
			continue;
		}

		if (arg == "--repl" || arg == "-I" || arg == "--interactive")  {
			enable_repl = true;
			continue;
		}

		if (arg == "-o" || arg == "--output") {
			if (args.empty()) {
				std::cerr << "musique: error: option " << arg << " requires an argument" << std::endl;
				std::exit(1);
			}
			output_port = pop<unsigned>(args);
			continue;
		}

		if (arg == "-h" || arg == "--help") {
			usage();
		}

		std::cerr << "musique: error: unrecognized command line option: " << arg << std::endl;
		std::exit(1);
	}

	Runner runner{output_port};

	for (auto const& [type, argument] : runnables) {
		if (type == Run::Argument) {
			Lines::the.add_line("<arguments>", argument, repl_line_number);
			Try(runner.run(argument, "<arguments>"));
			repl_line_number++;
			continue;
		}
		auto path = argument;
		if (path == "-") {
			eternal_sources.emplace_back(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
		} else {
			if (not fs::exists(path)) {
				std::cerr << "musique: error: couldn't open file: " << path << std::endl;
				std::exit(1);
			}
			std::ifstream source_file{fs::path(path)};
			eternal_sources.emplace_back(std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>());
		}

		Lines::the.add_file(std::string(path), eternal_sources.back());
		if (type == Run::File) {
			Try(runner.run(eternal_sources.back(), path));
		} else {
			Try(runner.deffered_file(eternal_sources.back(), argument));
		}
	}

	enable_repl = enable_repl || std::all_of(runnables.begin(), runnables.end(),
		[](Run const& run) {
			return run.type == Run::Deffered_File;
		});

	if (runnables.empty() || enable_repl) {
		repl_line_number = 1;
		enable_repl = true;
#ifndef _WIN32
		bestlineSetCompletionCallback(completion);
#else
		std::vector<std::string> repl_source_lines;
#endif
		for (;;) {
#ifndef _WIN32
			char const* input_buffer = bestlineWithHistory("> ", "musique");
			if (input_buffer == nullptr) {
				break;
			}
#else
			std::cout << "> " << std::flush;
			char const* input_buffer;
			if (std::string s{}; std::getline(std::cin, s)) {
				repl_source_lines.push_back(std::move(s));
				input_buffer = repl_source_lines.back().c_str();
			} else {
				break;
			}
#endif
			// Raw input line used for execution in language
			std::string_view raw = input_buffer;

			// Used to recognize REPL commands
			std::string_view command = raw;
			trim(command);

			if (command.empty()) {
				// Line is empty so there is no need to execute it or parse it
#ifndef _WIN32
				free(const_cast<char*>(input_buffer));
#else
				repl_source_lines.pop_back();
#endif
				continue;
			}

			if (command.starts_with(':')) {
				command.remove_prefix(1);
				if (!Try(handle_repl_session_commands(command, runner))) {
					std::cerr << "musique: error: unrecognized REPL command '" << command << '\'' << std::endl;
				}
				continue;
			}

			Lines::the.add_line("<repl>", raw, repl_line_number);
			auto result = runner.run(raw, "<repl>", true);
			using Traits = Try_Traits<std::decay_t<decltype(result)>>;
			if (not Traits::is_ok(result)) {
				std::cout << std::flush;
				std::cerr << Traits::yield_error(std::move(result)) << std::flush;
			}
			repl_line_number++;
			// We don't free input line since there could be values that still relay on it
		}
	}

	return {};
}

int main(int argc, char const** argv)
{
	auto const args = std::span(argv, argc).subspan(1);
	auto const result = Main(args);
	if (result.has_value()) {
		std::cerr << result.value() << std::flush;
		return 1;
	}
	return 0;
}
