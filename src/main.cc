#include <iostream>
#include <musique.hh>

#include <span>

static std::string_view pop(std::span<char const*> &span)
{
	auto element = span.front();
	span = span.subspan(1);
	return element;
}

Result<Unit> Main(std::span<char const*> args)
{
	while (not args.empty()) {
		std::string_view arg = pop(args);

		if (arg == "-c" || arg == "--run") {
			if (args.empty()) {
				std::cerr << "musique: error: option " << arg << " requires an argument" << std::endl;
				std::exit(1);
			}

			auto const source = pop(args);
			Try(Parser::parse(source, "arguments"));
			std::cout << "successfully parsed: " << source << " \n";
			continue;
		}

		std::cerr << "musique: error: unrecognized command line option: " << arg << std::endl;
		std::exit(1);
	}

	return {};
}

int main(int argc, char const** argv)
{
	auto result = Main(std::span{ argv+1, usize(argc-1) });
	if (not result.has_value()) {
		std::cerr << result.error() << std::flush;
		return 1;
	}
}
