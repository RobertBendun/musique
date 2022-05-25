#define MIDI_ENABLE_ALSA_SUPPORT

#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

#include <musique.hh>
#include <midi.hh>

namespace fs = std::filesystem;

static bool ast_only_mode = false;

static std::string_view pop(std::span<char const*> &span)
{
	auto element = span.front();
	span = span.subspan(1);
	return element;
}

[[noreturn]]
void usage()
{
	std::cerr <<
		"usage: musique <options> [filename]\n"
		"  where filename is path to file with Musique code that will be executed\n"
		"  where options are:\n"
		"    -c,--run CODE\n"
		"      executes given code\n"
		"    --ast\n"
		"      prints ast for given code\n"
		"    -l,--list\n"
		"      lists all available MIDI ports and quit\n";
	std::exit(1);
}

struct Runner
{
	midi::ALSA alsa;
	Interpreter interpreter;

	Runner(std::string port)
		: alsa("musique")
	{
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

	Result<void> run(std::string_view source, std::string_view filename)
	{
		auto ast = Try(Parser::parse(source, filename));

		if (ast_only_mode) {
			dump(ast);
			return {};
		}
		if (auto result = Try(interpreter.eval(std::move(ast))); result.type != Value::Type::Nil) {
			std::cout << result << std::endl;
		}
		return {};
	}
};

// We make sure that through life of interpreter source code is allways allocated
std::vector<std::string> eternal_sources;

struct Run
{
	bool is_file = true;
	std::string_view argument;
};

static Result<void> Main(std::span<char const*> args)
{
	if (args.empty()) {
		usage();
	}

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

		std::cerr << "musique: error: unrecognized command line option: " << arg << std::endl;
		std::exit(1);
	}

	if (runnables.empty()) {
		usage();
	}

	Runner runner("14");

	for (auto const& [is_file, argument] : runnables) {
		if (!is_file) {
			Try(runner.run(argument, "<arguments>"));
			continue;
		}
		auto const path = argument;
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

		Try(runner.run(eternal_sources.back(), path));
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
