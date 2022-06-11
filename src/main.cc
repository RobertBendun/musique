#define MIDI_ENABLE_ALSA_SUPPORT

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

#include <musique.hh>
#include <midi.hh>

extern "C" {
#include <bestline.h>
}

namespace fs = std::filesystem;

static bool ast_only_mode = false;
static bool enable_repl = false;

#define Ignore(Call) do { auto const ignore_ ## __LINE__ = (Call); (void) ignore_ ## __LINE__; } while(0)

/// Pop string from front of an array
static std::string_view pop(std::span<char const*> &span)
{
	auto element = span.front();
	span = span.subspan(1);
	return element;
}

/// Print usage and exit
[[noreturn]] void usage()
{
	std::cerr <<
		"usage: musique <options> [filename]\n"
		"  where filename is path to file with Musique code that will be executed\n"
		"  where options are:\n"
		"    -c,--run CODE\n"
		"      executes given code\n"
		"    -i,--interactive,--repl\n"
		"      enables interactive mode even when another code was passed\n"
		"    --ast\n"
		"      prints ast for given code\n"
		"    -l,--list\n"
		"      lists all available MIDI ports and quit\n";
	std::exit(1);
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

	midi::ALSA alsa;
	Interpreter interpreter;

	/// Setup interpreter and midi connection with given port
	Runner(std::string port)
		: alsa("musique")
	{
		assert(the == nullptr, "Only one instance of runner is supported");
		the = this;

		alsa.init_sequencer();
		alsa.connect(port);
		interpreter.midi_connection = &alsa;

		Env::global->force_define("say", +[](Interpreter&, std::vector<Value> args) -> Result<Value> {
			for (auto it = args.begin(); it != args.end(); ++it) {
			std::cout << *it;
				if (std::next(it) != args.end())
					std::cout << ' ';
			}
			std::cout << '\n';
			return {};
		});
	}

	Runner(Runner const&) = delete;
	Runner(Runner &&) = delete;
	Runner& operator=(Runner const&) = delete;
	Runner& operator=(Runner &&) = delete;

	/// Run given source
	Result<void> run(std::string_view source, std::string_view filename, bool output = false)
	{
		auto ast = Try(Parser::parse(source, filename));

		if (ast_only_mode) {
			dump(ast);
			return {};
		}
		if (auto result = Try(interpreter.eval(std::move(ast))); output && result.type != Value::Type::Nil) {
			std::cout << result << std::endl;
		}
		return {};
	}
};

/// All source code through life of the program should stay allocated, since
/// some of the strings are only views into source
std::vector<std::string> eternal_sources;

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

/// Fancy main that supports Result forwarding on error (Try macro)
static Result<void> Main(std::span<char const*> args)
{
	if (isatty(STDOUT_FILENO) && getenv("NO_COLOR") == nullptr) {
		pretty::terminal_mode();
	}

	/// Describes all arguments that will be run
	struct Run
	{
		bool is_file = true;
		std::string_view argument;
	};

	std::vector<Run> runnables;

	while (not args.empty()) {
		std::string_view arg = pop(args);

		if (arg == "-" || !arg.starts_with('-')) {
			runnables.push_back({ .is_file = true, .argument = std::move(arg) });
			continue;
		}

		if (arg == "-l" || arg == "--list") {
			midi::ALSA alsa("musique");
			alsa.init_sequencer();
			alsa.list_ports(std::cout);
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

		if (arg == "--repl" || arg == "-i" || arg == "--interactive")  {
			enable_repl = true;
			continue;
		}

		std::cerr << "musique: error: unrecognized command line option: " << arg << std::endl;
		std::exit(1);
	}

	Runner runner("14");

	for (auto const& [is_file, argument] : runnables) {
		if (!is_file) {
			Lines::the.add_line("<arguments>", argument);
			Try(runner.run(argument, "<arguments>"));
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
		enable_repl = true;
		bestlineSetCompletionCallback(completion);
		for (;;) {
			char *input_buffer = bestlineWithHistory("> ", "musique");
			if (input_buffer == nullptr) {
				break;
			}

			// Raw input line used for execution in language
			std::string_view raw = input_buffer;

			// Used to recognize REPL commands
			std::string_view command = raw;
			trim(command);

			if (command.empty()) {
				// Line is empty so there is no need to execute it or parse it
				free(input_buffer);
				continue;
			}

			if (command.starts_with(':')) {
				command.remove_prefix(1);
				if (command == "exit") { break; }
				if (command == "clear") { std::cout << "\x1b[1;1H\x1b[2J" << std::flush; continue; }
				if (command.starts_with('!')) { Ignore(system(command.data() + 1)); continue; }
				std::cerr << "musique: error: unrecognized REPL command '" << command << '\'' << std::endl;
				continue;
			}

			Lines::the.add_line("<repl>", raw);
			auto result = runner.run(raw, "<repl>", true);
			if (not result.has_value()) {
				std::cout << std::flush;
				std::cerr << result.error() << std::flush;
			}
			// We don't free input line since there could be values that still relay on it
		}
	}

	return {};
}

int main(int argc, char const** argv)
{
	auto const args = std::span(argv, argc).subspan(1);
	auto const result = Main(args);
	if (not result.has_value()) {
		std::cerr << result.error() << std::flush;
		return 1;
	}
}
