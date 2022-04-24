#include <iostream>
#include "musique.hh"

std::string_view Source = R"musique(
	nums = [ 1 2 3 ]
	say ( min nums + max nums )
)musique";

tl::expected<void, Error> Main()
{
	Lexer lexer{Source};

	for (;;) {
		auto token = Try(lexer.next_token());
		std::cout << token << '\n';
	}
}

int main()
{
	auto result = Main();
	if (not result.has_value()) {
		std::cerr << result.error() << std::endl;
		return 1;
	}
}
