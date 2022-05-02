#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

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
};
