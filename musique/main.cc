#include <charconv>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <edit_distance.hh>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <musique/cmd.hh>
#include <musique/format.hh>
#include <musique/interpreter/builtin_function_documentation.hh>
#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/lexer/lines.hh>
#include <musique/midi/midi.hh>
#include <musique/parser/parser.hh>
#include <musique/platform.hh>
#include <musique/pretty.hh>
#include <musique/try.hh>
#include <musique/unicode.hh>
#include <musique/user_directory.hh>
#include <musique/value/block.hh>
#include <span>
#include <thread>
#include <unordered_set>

#include <replxx.hxx>

#ifdef _WIN32
extern "C" {
#include <io.h>
}
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

bool ast_only_mode = false;
bool enable_repl = false;
static unsigned repl_line_number = 1;

#define Ignore(Call) do { auto const ignore_ ## __LINE__ = (Call); (void) ignore_ ## __LINE__; } while(0)

void print_repl_help()
{
	std::cout <<
		"List of all available commands of Musique interactive mode:\n"
		":exit - quit interactive mode\n"
		":help - prints this help message\n"
		":!<command> - allows for execution of any shell command\n"
		":clear - clears screen\n"
		":load <file> - loads file into Musique session\n"
		":ports - print list available ports"
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

	Interpreter interpreter;

	/// Setup interpreter and midi connection with given port
	explicit Runner()
		: interpreter{}
	{
		ensure(the == nullptr, "Only one instance of runner is supported");
		the = this;

		interpreter.current_context->connect(std::nullopt);

		Env::global->force_define("say", +[](Interpreter &interpreter, std::vector<Value> args) -> Result<Value> {
			for (auto it = args.begin(); it != args.end(); ++it) {
				std::cout << Try(format(interpreter, *it));
				if (std::next(it) != args.end())
					std::cout << ' ';
			}
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
		try {
			if (auto result = Try(interpreter.eval(std::move(ast))); output && not holds_alternative<Nil>(result)) {
				std::cout << Try(format(interpreter, result)) << std::endl;
			}
		} catch (KeyboardInterrupt const&) {
			interpreter.turn_off_all_active_notes();
			interpreter.starter.stop();
			std::cout << std::endl;
		}
		return {};
	}
};

/// All source code through life of the program should stay allocated, since
/// some of the strings are only views into source
std::vector<std::string> eternal_sources;

#if 0
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
		Command { "version",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::cout << Musique_Version << std::endl;
				return {};
			},
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

		Command {
			"ports",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				midi::Rt_Midi{}.list_ports(std::cout);
				return {};
			},
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

static Runner *runner;

void sigint_handler(int sig)
{
	if (sig == SIGINT) {
		runner->interpreter.issue_interrupt();
	}
	std::signal(SIGINT, sigint_handler);
}

/// Fancy main that supports Result forwarding on error (Try macro)
static std::optional<Error> Main(std::span<char const*> args)
{
	enable_repl = args.empty();

	if (cmd::is_tty() && getenv("NO_COLOR") == nullptr) {
		pretty::terminal_mode();
	}

	std::vector<cmd::Run> runnables;


	while (args.size()) if (auto failed = cmd::accept_commandline_argument(runnables, args)) {
		std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
		std::cerr << " Failed to recognize parameter " << std::quoted(*failed) << std::endl;
		cmd::print_close_matches(args.front());
		std::exit(1);
	}

	Runner runner;
	::runner = &runner;
	std::signal(SIGINT, sigint_handler);

	for (auto const& [type, argument] : runnables) {
		if (type == cmd::Run::Argument) {
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
				std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
				std::cerr << " couldn't open file: " << path << std::endl;
				std::exit(1);
			}
			std::ifstream source_file{fs::path(path)};
			eternal_sources.emplace_back(std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>());
		}

		Lines::the.add_file(std::string(path), eternal_sources.back());
		if (type == cmd::Run::File) {
			Try(runner.run(eternal_sources.back(), path));
		} else {
			Try(runner.deffered_file(eternal_sources.back(), argument));
		}
	}

	enable_repl = enable_repl || (!runnables.empty() && std::all_of(runnables.begin(), runnables.end(),
		[](cmd::Run const& run) { return run.type == cmd::Run::Deffered_File; }));

	if (enable_repl) {
		repl_line_number = 1;


		replxx::Replxx repl;

		auto const history_path = (user_directory::data_home() / "history").string();

		repl.set_max_history_size(2048);
		repl.set_max_hint_rows(3);
		repl.history_load(history_path);

		for (;;) {
			char const* input = nullptr;
			do input = repl.input("> "); while((input == nullptr) && (errno == EAGAIN));

			if (input == nullptr) {
				break;
			}

			// Raw input line used for execution in language
			std::string_view raw = input;

			// Used to recognize REPL commands
			std::string_view command = raw;
			trim(command);

			if (command.empty()) {
				continue;
			}

			repl.history_add(std::string(command));
			repl.history_save(history_path);

			if (command.starts_with(':')) {
				command.remove_prefix(1);
				if (!Try(handle_repl_session_commands(command, runner))) {
					std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
					std::cerr << " unrecognized REPL command '" << command << '\'' << std::endl;
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
