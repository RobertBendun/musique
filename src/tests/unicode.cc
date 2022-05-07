#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

void test_encoding(
		u32 rune,
		std::string_view expected,
		reflection::source_location sl = reflection::source_location::current())
{
	std::stringstream ss;
	ss << utf8::Print{rune};
	expect(eq(ss.str(), expected), sl);
}

suite utf8_test = [] {
	"UTF-8 Character length"_test = [] {
		expect(utf8::length(" ")          == 1_u);
		expect(utf8::length("ą")          == 2_u);
		expect(utf8::length("\u2705")     == 3_u);
		expect(utf8::length("\U000132d1") == 4_u);
	};

	"UTF-8 Character decoding"_test = [] {
		expect(eq(utf8::decode(" ").first,          0x20u));
		expect(eq(utf8::decode("ą").first,          0x105u));
		expect(eq(utf8::decode("\u2705").first,     0x2705u));
		expect(eq(utf8::decode("\U000132d1").first, 0x132d1u));
	};

	"UTF-8 Character encoding"_test = [] {
		test_encoding(0x20u,    " ");
		test_encoding(0x105u,   "ą");
		test_encoding(0x2705u,  "\u2705");
		test_encoding(0x132d1u, "\U000132d1");
	};
};
