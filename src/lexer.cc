#include <musique.hh>

auto Lexer::next_token() -> Result<Token>
{
	auto current = source;

	auto c = next_rune();

	if (c == 0)
		return errors::End_Of_File;

	switch (c) {
	case '(': return { Token::Type::Open_Paren,         current.substr(0, 1) };
	case ')': return { Token::Type::Close_Paren,        current.substr(0, 1) };
	case '[': return { Token::Type::Open_Block,         current.substr(0, 1) };
	case ']': return { Token::Type::Close_Block,        current.substr(0, 1) };
	case '|': return { Token::Type::Variable_Separator, current.substr(0, 1) };
	}

	// Number literals like .75
	if (c == '.') {
		while ((c = next_rune()) && std::isdigit(c)) {}
		if (source.data() - current.data() != 1)
			return { Token::Type::Numeric, current.substr(0, source.data() - current.data()) };
	}

	if (std::isdigit(c)) {
		while ((c = next_rune()) && std::isdigit(c)) {}
		if (c == '.') {
			bool looped = false;
			while ((c = next_rune()) && std::isdigit(c)) { looped = true; }
			if (not looped) {
				// If '.' is not followed by any digits, then '.' is not part of numeric literals
				// and only part before it is considered valid
				rewind();
			}
		}
		return { Token::Type::Numeric, current.substr(0, source.data() - current.data()) };
	}


	return {};
}

auto Lexer::next_rune() -> u32
{
	if (not source.empty()) {
		if (auto [rune, remaining] = utf8::decode(source); rune != utf8::Rune_Error) {
			last_rune_length = remaining.data() - source.data();
			source = remaining;
			return rune;
		}
	}
	return 0;
}

void Lexer::rewind()
{
	source = { source.data() - last_rune_length, source.size() + last_rune_length };
}

std::ostream& operator<<(std::ostream& os, Token const&)
{
	os << "Token";
	return os;
}
