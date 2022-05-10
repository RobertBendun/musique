#include <iostream>
#include <musique.hh>

#include <filesystem>
#include <span>
#include <fstream>

namespace fs = std::filesystem;

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
		"    -c CODE\n"
		"    --run CODE\n"
		"      executes given code\n";
	std::exit(1);
}

static Result<void> run(std::string_view source, std::string_view filename)
{
	auto ast = Try(Parser::parse(source, filename));
	std::cout << "successfully parsed: " << source << " \n";
	dump(ast);
	return {};
}

// We make sure that through life of interpreter source code is allways allocated
std::vector<std::string> eternal_sources;

static Result<void> Main(std::span<char const*> args)
{
	if (args.empty()) {
		usage();
	}

	std::vector<std::string_view> files;

	while (not args.empty()) {
		std::string_view arg = pop(args);

		if (!arg.starts_with('-')) {
			files.push_back(std::move(arg));
			continue;
		}

		if (arg == "-c" || arg == "--run") {
			if (args.empty()) {
				std::cerr << "musique: error: option " << arg << " requires an argument" << std::endl;
				std::exit(1);
			}

			auto const source = pop(args);
			Try(run(source, "arguments"));
			continue;
		}

		std::cerr << "musique: error: unrecognized command line option: " << arg << std::endl;
		std::exit(1);
	}

	for (auto const& path : files) {
		if (not fs::exists(path)) {
			std::cerr << "musique: error: couldn't open file: " << path << std::endl;
			std::exit(1);
		}

		{
			std::ifstream source_file{fs::path(path)};
			eternal_sources.emplace_back(std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>());
		}

		Try(run(eternal_sources.back(), path));
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
