#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <thread>
#include <cstring>

#include <musique/format.hh>
#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/lexer/lines.hh>
#include <musique/midi/midi.hh>
#include <musique/parser/parser.hh>
#include <musique/pretty.hh>
#include <musique/try.hh>
#include <musique/unicode.hh>

namespace fs = std::filesystem;

static bool ast_only_mode = false;
static bool enable_repl = false;
static unsigned repl_line_number = 1;

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
		"    -i,--input PORT\n"
		"      provides input port, a place where Musique receives MIDI messages\n"
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

/// Runs interpreter on given source code
struct Runner
{
	static inline Runner *the;

	midi::Rt_Midi midi;
	Interpreter interpreter;
	std::thread midi_input_event_loop;
	std::stop_source stop_source;

	/// Setup interpreter and midi connection with given port
	Runner(std::optional<unsigned> input_port, std::optional<unsigned> output_port)
		: midi()
		, interpreter{}
	{
		assert(the == nullptr, "Only one instance of runner is supported");
		the = this;

		bool const midi_go = bool(input_port) || bool(output_port);
		if (midi_go) {
			interpreter.midi_connection = &midi;
		}
		if (output_port) {
			std::cout << "Connected MIDI output to port " << *output_port << ". Ready to play!" << std::endl;
			midi.connect_output(*output_port);
		}
		if (input_port) {
			std::cout << "Connected MIDI input to port " << *input_port << ". Ready for incoming messages!" << std::endl;
			midi.connect_input(*input_port);
		}
		if (midi_go) {
			interpreter.register_callbacks();
			midi_input_event_loop = std::thread([this] { handle_midi_event_loop(); });
			midi_input_event_loop.detach();
		}

		Env::global->force_define("say", +[](Interpreter &interpreter, std::vector<Value> args) -> Result<Value> {
			for (auto it = args.begin(); it != args.end(); ++it) {
				std::cout << Try(format(interpreter, *it));
				if (std::next(it) != args.end())
					std::cout << ' ';
			}
			std::cout << '\n';
			return {};
		});
	}

	~Runner()
	{
		stop_source.request_stop();
	}

	Runner(Runner const&) = delete;
	Runner(Runner &&) = delete;
	Runner& operator=(Runner const&) = delete;
	Runner& operator=(Runner &&) = delete;

	void handle_midi_event_loop()
	{
		midi.input_event_loop(stop_source.get_token());
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

bool is_tty()
{
#ifdef __linux__
	return isatty(STDOUT_FILENO);
#else
	return true;
#endif
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
		bool is_file = true;
		std::string_view argument;
	};

	// Arbitraly chosen for conviniance of the author
	std::optional<unsigned> input_port{};
	std::optional<unsigned> output_port{};

	std::vector<Run> runnables;

	while (not args.empty()) {
		std::string_view arg = pop(args);

		if (arg == "-" || !arg.starts_with('-')) {
			runnables.push_back({ .is_file = true, .argument = std::move(arg) });
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
			runnables.push_back({ .is_file = false, .argument = pop(args) });
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

		if (arg == "-i" || arg == "--input") {
			if (args.empty()) {
				std::cerr << "musique: error: option " << arg << " requires an argument" << std::endl;
				std::exit(1);
			}
			input_port = pop<unsigned>(args);
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

	Runner runner{input_port, output_port};

	for (auto const& [is_file, argument] : runnables) {
		if (!is_file) {
			Lines::the.add_line("<arguments>", argument, repl_line_number);
			Try(runner.run(argument, "<arguments>"));
			repl_line_number++;
			continue;
		}
		auto path = argument;
		if (path == "-") {
			path = "<stdin>";
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
		Try(runner.run(eternal_sources.back(), path));
	}

	if (runnables.empty() || enable_repl) {
		repl_line_number = 1;
		enable_repl = true;
		for (;;) {
			std::string_view raw;
			if (auto s = new std::string{}; std::getline(std::cin, *s)) {
				raw = *s;
			} else {
				break;
			}

			// Used to recognize REPL commands
			std::string_view command = raw;
			trim(command);

			if (command.empty()) {
				// Line is empty so there is no need to execute it or parse it
				continue;
			}

			if (command.starts_with(':')) {
				command.remove_prefix(1);
				if (command == "exit") { break; }
				if (command == "clear") { std::cout << "\x1b[1;1H\x1b[2J" << std::flush; continue; }
				if (command.starts_with('!')) { Ignore(system(command.data() + 1)); continue; }
				if (command == "help") { print_repl_help(); continue; }
				std::cerr << "musique: error: unrecognized REPL command '" << command << '\'' << std::endl;
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
}
