#ifndef MUSIQUE_UNICODE_HH
#define MUSIQUE_UNICODE_HH

#include <musique/common.hh>
#include <ostream>

/// All unicode related operations
namespace unicode
{
	inline namespace special_runes
	{
		[[maybe_unused]] constexpr u32 Rune_Error = 0xfffd;
		[[maybe_unused]] constexpr u32 Rune_Self  = 0x80;
		[[maybe_unused]] constexpr u32 Max_Bytes  = 4;
	}

	/// is_digit returns true if `digit` is ASCII digit
	bool is_digit(u32 digit);

	/// is_space return true if `space` is ASCII blank character (EXCLUDING newline)
	bool is_space(u32 space);

	/// is_space_or_newline returns if `space` is ASCII blank character (including newline)
	bool is_space_or_newline(u32 space);

	/// is_letter returns true if `letter` is considered a letter by Unicode
	bool is_letter(u32 letter);

	/// is_identifier returns true if `letter` is valid character for identifier.
	///
	/// It's modifier by is_first_character flag to determine some character classes
	/// allowance like numbers, which are only allowed NOT at the front of the identifier
	enum class First_Character : bool { Yes = true, No = false };
	bool is_identifier(u32 letter, First_Character is_first_character);
}

/// utf8 encoding and decoding
namespace utf8
{
	using namespace unicode::special_runes;

	/// Decodes rune and returns remaining string
	auto decode(std::string_view s) -> std::pair<u32, std::string_view>;

	/// Returns length of the first rune in the provided string
	auto length(std::string_view s) -> usize;

	/// Encodes rune and returns as string
	auto encode(u32 rune) -> std::string;

	struct Print { u32 rune; };
}

std::ostream& operator<<(std::ostream& os, utf8::Print const& print);

#endif
