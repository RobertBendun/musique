#include <musique.hh>

auto Lexer::next_token() -> Result<Token>
{
	while (consume_if(unicode::is_space)) {
	}
	start();

	if (peek() == 0) {
		return errors::End_Of_File;
	}

	switch (peek()) {
	case '(': consume(); return { Token::Type::Open_Paren,         finish() };
	case ')': consume(); return { Token::Type::Close_Paren,        finish() };
	case '[': consume(); return { Token::Type::Open_Block,         finish() };
	case ']': consume(); return { Token::Type::Close_Block,        finish() };
	case '|': consume(); return { Token::Type::Variable_Separator, finish() };
	}

	// Number literals like .75
	if (peek() == '.') {
		consume();
		while (consume_if(unicode::is_digit)) {}
		if (token_length != 1)
			return { Token::Type::Numeric, finish() };
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
		return { Token::Type::Numeric, finish() };
	}

	return errors::Unrecognized_Character;
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
	if (not source.empty()) {
		if (auto [rune, remaining] = utf8::decode(source); rune != utf8::Rune_Error) {
			last_rune_length = remaining.data() - source.data();
			source = remaining;
			token_length += last_rune_length;
			return rune;
		}
	}
	return 0;
}

void Lexer::rewind()
{
	source = { source.data() - last_rune_length, source.size() + last_rune_length };
	token_length -= last_rune_length;
}

void Lexer::start()
{
	token_start = source.data();
	token_length = 0;
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
