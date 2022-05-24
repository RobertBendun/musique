#include <musique.hh>

#include <iomanip>

constexpr std::string_view Notes_Symbols = "abcedefgh";
constexpr std::string_view Valid_Operator_Chars =
	"+-*/:%" // arithmetic
	"|&^"    // logic & bit operations
	"<>=!"   // comparisons
	"."      // indexing
	;

constexpr auto Keywords = std::array {
	"false"sv,
	"nil"sv,
	"true"sv,
	"var"sv
};

void Lexer::skip_whitespace_and_comments()
{
	for (;;) {
		bool done_something = false;

		while (consume_if(unicode::is_space)) {
			done_something = true;
		}

		// #! line comments
		if (consume_if('#', '!')) {
			done_something = true;
			while (peek() && peek() != '\n') {
				consume();
			}
		}

		// -- line and multiline coments
		if (consume_if('-', '-')) {
			done_something = true;
			if (consume_if('-')) {
				// multiline
				unsigned count = 0;
				while (count < 3) if (consume_if('-')) {
					++count;
				} else {
					consume();
					count = 0;
				}
				while (consume_if('-')) {}
			} else {
				// single line
				while (peek() && peek() != '\n') {
					consume();
				}
			}
		}

		if (not done_something)
			break;
	}
}

auto Lexer::next_token() -> Result<Token>
{
	skip_whitespace_and_comments();
	start();

	if (peek() == 0) {
		return errors::End_Of_File;
	}

	switch (peek()) {
	case '(': consume(); return Token { Token::Type::Open_Paren,           finish(), token_location };
	case ')': consume(); return Token { Token::Type::Close_Paren,          finish(), token_location };
	case '[': consume(); return Token { Token::Type::Open_Block,           finish(), token_location };
	case ']': consume(); return Token { Token::Type::Close_Block,          finish(), token_location };
	case ';': consume(); return Token { Token::Type::Expression_Separator, finish(), token_location };

	case '|':
		consume();
		// `|` may be part of operator, like `||`. So we need to check what follows. If next char
		// is operator, then this character is part of operator sequence.
		// Additionally we explicitly allow for `|foo|=0` here
		if (Valid_Operator_Chars.find(peek()) == std::string_view::npos || peek() == '=')
			return Token { Token::Type::Parameter_Separator, finish(), token_location };
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
		return Token { Token::Type::Numeric, finish(), token_location };
	}

	// lex chord declaration
	if (consume_if(Notes_Symbols)) {
		// Allow `c#`
		consume_if('#');

		while (consume_if('_') || consume_if(unicode::is_digit)) {}

		// If we encounter any letter that is not part of chord declaration,
		// then we have symbol, not chord declaration
		if (unicode::is_identifier(peek(), unicode::First_Character::No)) {
			goto symbol_lexing;
		}

		return Token { Token::Type::Chord, finish(), token_location };
	}

	using namespace std::placeholders;
	if (consume_if(std::bind(unicode::is_identifier, _1, unicode::First_Character::Yes))) {
	symbol_lexing:
		for (auto predicate = std::bind(unicode::is_identifier, _1, unicode::First_Character::No);
				consume_if(predicate);) {}

		Token t = { Token::Type::Symbol, finish(), token_location };
		if (std::find(Keywords.begin(), Keywords.end(), t.source) != Keywords.end()) {
			t.type = Token::Type::Keyword;
		} else if (t.source == "v") {
			t.type = Token::Type::Operator;
		}
		return t;
	}

	if (consume_if(Valid_Operator_Chars)) {
		while (consume_if(Valid_Operator_Chars)) {}
		return Token { Token::Type::Operator, finish(), token_location };
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

auto Lexer::consume_if(auto test) -> bool
{
	bool condition;
	if constexpr (requires { test(peek()) && true; }) {
		condition = test(peek());
	} else if constexpr (std::is_integral_v<decltype(test)>) {
		condition = (u32(test) == peek());
	} else if constexpr (std::is_convertible_v<decltype(test), char const*>) {
		auto const end = test + std::strlen(test);
		condition = std::find(test, end, peek()) != end;
	} else {
		condition = std::find(std::begin(test), std::end(test), peek()) != std::end(test);
	}
	return condition && (consume(), true);
}

auto Lexer::consume_if(auto first, auto second) -> bool
{
	if (consume_if(first)) {
		if (consume_if(second)) {
			return true;
		} else {
			rewind();
		}
	}
	return false;
}

void Lexer::rewind()
{
	assert(last_rune_length != 0, "cannot rewind to not existing rune");
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

std::ostream& operator<<(std::ostream& os, Token const& token)
{
	return os << '{' << token.type << ", " << std::quoted(token.source) << ", " << token.location << '}';
}

std::ostream& operator<<(std::ostream& os, Token::Type type)
{
	switch (type) {
	case Token::Type::Chord:                 return os << "CHORD";
	case Token::Type::Close_Block:           return os << "CLOSE BLOCK";
	case Token::Type::Close_Paren:           return os << "CLOSE PAREN";
	case Token::Type::Expression_Separator:  return os << "EXPRESSION SEPARATOR";
	case Token::Type::Keyword:               return os << "KEYWORD";
	case Token::Type::Numeric:               return os << "NUMERIC";
	case Token::Type::Open_Block:            return os << "OPEN BLOCK";
	case Token::Type::Open_Paren:            return os << "OPEN PAREN";
	case Token::Type::Operator:              return os << "OPERATOR";
	case Token::Type::Parameter_Separator:   return os << "PARAMETER SEPARATOR";
	case Token::Type::Symbol:                return os << "SYMBOL";
	}
	unreachable();
}
