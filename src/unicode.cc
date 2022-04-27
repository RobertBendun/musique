#include "musique.hh"

auto utf8::decode(std::string_view s) -> std::pair<u32, std::string_view>
{
	static constexpr std::array<u8, 4> payloads {
		0b0111'1111, 0b0001'1111, 0b0000'1111, 0b0000'0111
	};

	static constexpr std::array<u8, 4> patterns {
		0b0000'0000, 0b1100'0000, 0b1110'0000, 0b1111'0000
	};

	constexpr auto payload_cont  = 0b0011'1111;
	constexpr auto pattern_cont  = 0b1000'0000;

	if (s.empty()) {
		return { utf8::Rune_Error, s };
	}

	usize length = 0;

	for (auto i = 0u; i < payloads.size(); ++i) {
		if ((s.front() & ~payloads[i]) == patterns[i]) {
			length = i+1;
			break;
		}
	}

	if (length == 0 || s.size() < length) {
		return { utf8::Rune_Error, s };
	}

	u32 result = s.front() & payloads[length-1];

	while (--length > 0) {
		s.remove_prefix(1);
		if ((s.front() & ~payload_cont) == pattern_cont)
			return { utf8::Rune_Error, s };

		result <<= 6;
		result |= u32(s.front() & payload_cont);
	}

	s.remove_prefix(1);

	return { result, s };
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
