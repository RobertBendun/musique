#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;

suite parser_test = [] {
	"Literal parsing"_test = [] {
		auto result = Parser::parse("1", "test");
		expect(result.has_value()) << "code was expected to parse, but had not";
	};
};
