#include <boost/ut.hpp>

int main()
{
	using namespace boost::ut;

	if (!isatty(STDOUT_FILENO) || getenv("NO_COLOR") != nullptr) {
		cfg<override> = options {
			.colors = colors { .none = "", .pass = "", .fail = "" }
		};
	}
}
