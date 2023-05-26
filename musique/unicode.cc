#include <array>
#include <musique/unicode.hh>
#include <tuple>

static constexpr std::array<u8, 4> payloads {
	0b0111'1111, 0b0001'1111, 0b0000'1111, 0b0000'0111
};

static constexpr std::array<u8, 4> patterns {
	0b0000'0000, 0b1100'0000, 0b1110'0000, 0b1111'0000
};

constexpr auto payload_cont  = 0b0011'1111;
constexpr auto pattern_cont  = 0b1000'0000;

auto utf8::length(std::string_view s) -> usize
{
	if (not s.empty()) {
		for (auto i = 0u; i < payloads.size(); ++i) {
			if ((u8(s.front()) & ~payloads[i]) == patterns[i]) {
				return i+1;
			}
		}
	}
	return 0;
}

auto utf8::decode(std::string_view s) -> std::pair<u32, std::string_view>
{
	if (s.empty()) {
		return { utf8::Rune_Error, s };
	}

	usize length = utf8::length(s);

	if (length == 0 || s.size() < length) {
		return { utf8::Rune_Error, s };
	}

	u32 result = s.front() & payloads[length-1];

	while (--length > 0) {
		s.remove_prefix(1);
		if ((s.front() & ~payload_cont) == pattern_cont) {
			return { utf8::Rune_Error, s };
		}
		result <<= 6;
		result |= u32(s.front() & payload_cont);
	}

	s.remove_prefix(1);

	return { result, s };
}

static std::tuple<std::array<std::uint8_t, utf8::Max_Bytes>, std::size_t> utf8_encode(u32 r)
{
	std::array<u8, utf8::Max_Bytes> buffer;
	unsigned length = 0;

	if (r <= 0x7f) {
		buffer[0] = r;
		length = 1;
	} else if (r <= 0x07'ff) {
		buffer[0] = ((r >> 6) & 0x1f) | 0xc0;
		buffer[1] = ((r >> 0) & 0x3f) | 0x80;
		length = 2;
	} else if (r <= 0xff'ff) {
		buffer[0] = ((r >> 12) & 0x0f) | 0xe0;
		buffer[1] = ((r >>  6) & 0x3f) | 0x80;
		buffer[2] = ((r >>  0) & 0x3f) | 0x80;
		length = 3;
	} else {
		buffer[0] = ((r >> 18) & 0x07) | 0xf0;
		buffer[1] = ((r >> 12) & 0x3f) | 0x80;
		buffer[2] = ((r >>  6) & 0x3f) | 0x80;
		buffer[3] = ((r >>  0) & 0x3f) | 0x80;
		length = 4;
	}

	return {buffer, length};
}

auto utf8::encode(u32 r) -> std::string
{
	auto [buffer, length] = utf8_encode(r);
	return std::string(reinterpret_cast<char const*>(buffer.data()), length);
}

std::ostream& operator<<(std::ostream& os, utf8::Print const& print)
{
	auto [buffer, length] = utf8_encode(print.rune);
	return os.write(reinterpret_cast<char const*>(buffer.data()), length);
}

bool unicode::is_digit(u32 digit)
{
	return digit >= '0' && digit <= '9';
}

bool unicode::is_space(u32 space)
{
	switch (space) {
	case ' ':
	case '\t':
	case '\n':
	case '\f':
	case '\r':
	case '\v':
		return true;
	}
	return false;
}

bool unicode::is_identifier(u32 rune, unicode::First_Character is_first_character)
{

	return (unicode::is_letter(rune)
		|| rune == '\''
		|| rune == '_'
		|| rune == '#'
		|| rune == '$'
		|| rune == '@'
		// This characters are not consider identifiers by Unicode but it would be a shame to not include them
		// see: https://en.wikipedia.org/wiki/Musical_Symbols_(Unicode_block)
		|| (rune >= 0x1d15d /* aka whole note symbol '𝅝' */ && rune <= 0x1d164 /* aka semihemidemisemiquaver note '𝅘𝅥𝅲' */)
		|| (rune >= 0x1d13b /* aka whole rest symbol '𝄻' */ && rune <= 0x1d142 /* aka semihemidemisemiquaver rest '𝅂' */)
	) || (!bool(is_first_character) && unicode::is_digit(rune));
}
