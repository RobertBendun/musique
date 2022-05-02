#include <musique.hh>

constexpr std::string_view Notes_Symbols = "abcedefgh";

auto Lexer::next_token() -> Result<Token>
{
	while (consume_if(unicode::is_space)) {}
	start();

	if (peek() == 0) {
		return errors::End_Of_File;
	}

	switch (peek()) {
	case '(': consume(); return { Token::Type::Open_Paren,         finish(), token_location };
	case ')': consume(); return { Token::Type::Close_Paren,        finish(), token_location };
	case '[': consume(); return { Token::Type::Open_Block,         finish(), token_location };
	case ']': consume(); return { Token::Type::Close_Block,        finish(), token_location };
	case '|': consume(); return { Token::Type::Variable_Separator, finish(), token_location };
	}

	// Number literals like .75
	if (peek() == '.') {
		consume();
		while (consume_if(unicode::is_digit)) {}
		if (token_length != 1)
			return { Token::Type::Numeric, finish(), token_location };
	}

	if (consume_if(unicode::is_digit)) {
		while (consume_if(unicode::is_digit)) {}
		if (peek() == '.') {
			consume();
			bool looped = false;
			while (consume_if(unicode::is_digit)) { looped = true; }
			if (not looped) {
				// If '.' is not followed by any digits, then '.' is not part of numeric literals
				// and only part before it is considered valid
				rewind();
			}
		}
		return { Token::Type::Numeric, finish(), token_location };
	}

	if (consume_if([](u32 ch) { return Notes_Symbols.find(ch) != std::string_view::npos; })) {
		// chord declaration
		constexpr u8 Expect_Number         = 0b01;
		constexpr u8 Expect_Move           = 0b10;
		constexpr u8 Expect_Number_Or_Move = 0b11;

		auto current = Expect_Number;
		std::string_view accepted_digits = "12357";
		usize digit_cursor = 0;

		for (;;) {
			if ((current & Expect_Move) == Expect_Move
			&& consume_if([](u32 c) { return c == ',' || c == '\''; })
			) {
				current = Expect_Number;
				continue;
			}

			if ((current & Expect_Number) == Expect_Number) {
				bool found = false;
				for (; digit_cursor < accepted_digits.size(); ++digit_cursor) {
					if (consume_if([&](u32 c) { return u32(accepted_digits[digit_cursor]) == c; })) {
						found = true;
						break;
					}
				}

				if (found) {
					current = digit_cursor < accepted_digits.size()
						? Expect_Number_Or_Move
						: Expect_Move;
					continue;
				}
			}

			break;
		}

		if (unicode::is_identifier(peek(), unicode::First_Character::No)) {
			goto symbol_lexing;
		}

		return { Token::Type::Chord, finish(), token_location };
	}

	using namespace std::placeholders;
	if (consume_if(std::bind(unicode::is_identifier, _1, unicode::First_Character::Yes))) {
	symbol_lexing:
		for (auto predicate = std::bind(unicode::is_identifier, _1, unicode::First_Character::No);
				consume_if(predicate);
		) {
		}

		return { Token::Type::Symbol, finish(), token_location };
	}

	return errors::unrecognized_character(peek(), token_location);
}

auto Lexer::peek() const -> u32
{
	if (not source.empty()) {
		if (auto [rune, remaining] = utf8::decode(source); rune != utf8::Rune_Error) {
			return rune;
		}
	}
	return 0;
}

auto Lexer::consume() -> u32
{
	prev_location = location;
	if (not source.empty()) {
		if (auto [rune, remaining] = utf8::decode(source); rune != utf8::Rune_Error) {
			last_rune_length = remaining.data() - source.data();
			source = remaining;
			token_length += last_rune_length;
			location.advance(rune);
			return rune;
		}
	}
	return 0;
}

void Lexer::rewind()
{
	assert(last_rune_length != 0);
	source = { source.data() - last_rune_length, source.size() + last_rune_length };
	token_length -= last_rune_length;
	location = prev_location;
	last_rune_length = 0;
}

void Lexer::start()
{
	token_start = source.data();
	token_length = 0;
	token_location = location;
}

std::string_view Lexer::finish()
{
	std::string_view result { token_start, token_length };
	token_start = nullptr;
	token_length = 0;
	return result;
}

std::ostream& operator<<(std::ostream& os, Token const&)
{
	os << "Token";
	return os;
}

Location Location::advance(u32 rune)
{
	switch (rune) {
	case '\n':
		line += 1;
		[[fallthrough]];
	case '\r':
		column = 1;
		return *this;
	}
	column += 1;
	return *this;
}

std::ostream& operator<<(std::ostream& os, Location const& location)
{
	return os << location.filename << ':' << location.line << ':' << location.column;
}
