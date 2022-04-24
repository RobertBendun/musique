#include <boost/ut.hpp>

int main()
{
	using namespace boost::ut;

	if (!isatty(STDOUT_FILENO)) {
		cfg<override> = options {
			.colors = colors { .none = "", .pass = "", .fail = "" }
		};
	}
}
