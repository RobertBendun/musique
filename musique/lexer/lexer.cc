#include <musique/lexer/lexer.hh>
#include <musique/unicode.hh>

#include <iomanip>
#include <cstring>

constexpr std::string_view Notes_Symbols = "abcedefgp";
constexpr std::string_view Valid_Operator_Chars =
	"+-*/:%" // arithmetic
	"&^"    // logic & bit operations
	"<>=!"   // comparisons
	"."      // indexing
	;


constexpr auto Keywords = std::array {
	std::pair { "and"sv,   Token::Keyword::And   },
	std::pair { "do"sv,    Token::Keyword::Do    },
	std::pair { "else"sv,  Token::Keyword::Else  },
	std::pair { "end"sv,   Token::Keyword::End   },
	std::pair { "false"sv, Token::Keyword::False },
	std::pair { "for"sv,   Token::Keyword::For   },
	std::pair { "if"sv,    Token::Keyword::If    },
	std::pair { "nil"sv,   Token::Keyword::Nil   },
	std::pair { "or"sv,    Token::Keyword::Or    },
	std::pair { "then"sv,  Token::Keyword::Then  },
	std::pair { "true"sv,  Token::Keyword::True  },
	std::pair { "while"sv, Token::Keyword::While },
};

static_assert(Keywords.size() == Keywords_Count, "Table above should contain all the tokens for lexing");

void Lexer::skip_whitespace_and_comments()
{
	// Comments in this language have two kinds:
	//   Single line comment that starts with either '--' or '#!' and ends with end of line
	//   Multiline comments that start with at least 3 '-' and ends with at least 3 '-'
	// This function skips over all of them

	for (;;) {
		{ // Skip whitespace
			bool done_something = false;
			while (consume_if(unicode::is_space)) done_something = true;
			if (done_something) continue;
		}

		{ // Skip #! comments
			if (consume_if('#', '!')) {
				while (peek() && peek() != '\n') consume();
				continue;
			}
		}

		// Skip -- line and multiline coments
		if (consume_if('-', '-')) {
			if (!consume_if('-')) {
				// skiping a single line comment requires advancing until newline
				while (peek() && peek() != '\n') consume();
				continue;
			}

			// multiline comments begins with at least 3 '-' and ends with at least 3 '-'
			// so when we encounter 3 '-', we must explicitly skip remaining '-', so they
			// wil not end multiline comment

			// skip remaining from multiline comment begining
			while (consume_if('-')) {}

			{ // parse multiline comment until '---' marking end
				unsigned count = 0;
				while (count < 3) if (consume_if('-')) {
					++count;
				} else {
					consume();
					count = 0;
				}
				// skip remaining from multiline comment ending
				while (consume_if('-')) {}
			}
			continue;
		}

		// When we reached this place, we didn't skip any whitespace or comment
		// so our work is done
		break;
	}
}

auto Lexer::next_token() -> Result<std::variant<Token, End_Of_File>>
{
	skip_whitespace_and_comments();
	start();

	if (peek() == 0) {
		return End_Of_File{};
	}

	switch (peek()) {
	case '(':  consume(); return Token { .type = Token::Type::Open_Paren,          .start = token_location, .source = finish() };
	case ')':  consume(); return Token { .type = Token::Type::Close_Paren,         .start = token_location, .source = finish() };
	case ',':  consume(); return Token { .type = Token::Type::Comma,               .start = token_location, .source = finish() };
	case '[':  consume(); return Token { .type = Token::Type::Open_Bracket,        .start = token_location, .source = finish() };
	case '\n': consume(); return Token { .type = Token::Type::Nl,                  .start = token_location, .source = finish() };
	case ']':  consume(); return Token { .type = Token::Type::Close_Bracket,       .start = token_location, .source = finish() };
	case '|':  consume(); return Token { .type = Token::Type::Parameter_Separator, .start = token_location, .source = finish() };
	}

	// Lex numeric literals
	// They may have following forms: 0, 0.1
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
		return Token { .type = Token::Type::Numeric, .start = token_location, .source = finish() };
	}

	// Lex chord declaration
	if (consume_if(Notes_Symbols)) {
		while (consume_if("#sfb")) {}

		while (consume_if(unicode::is_digit)) {}

		// If we encounter any letter that is not part of chord declaration,
		// then we have symbol, not chord declaration
		if (unicode::is_identifier(peek(), unicode::First_Character::No)) {
			goto symbol_lexing;
		}

		return Token { .type = Token::Type::Chord, .start = token_location, .source = finish() };
	}

	using namespace std::placeholders;

	// Lex quoted symbol
	if (consume_if('\'')) {
		for (auto predicate = std::bind(unicode::is_identifier, _1, unicode::First_Character::No);
				consume_if(predicate) || consume_if(Valid_Operator_Chars);) {}

		Token t = { .type = Token::Type::Symbol, .start = token_location, .source = finish() };
		return t;
	}

	// Lex symbol
	if (consume_if(std::bind(unicode::is_identifier, _1, unicode::First_Character::Yes))) {
	symbol_lexing:
		for (auto predicate = std::bind(unicode::is_identifier, _1, unicode::First_Character::No);
				consume_if(predicate);) {}

		Token t = { .type = Token::Type::Symbol, .start = token_location, .source = finish() };
		for (auto const& [keyword_name, keyword_type] : Keywords) {
			if (keyword_name == t.source) {
				t.type = Token::Type::Keyword;
				t.keyword_type = keyword_type;
				return t;
			}
		}
		return t;
	}

	// Lex operator
	if (consume_if(Valid_Operator_Chars)) {
		while (consume_if(Valid_Operator_Chars)) {}
		return Token { .type = Token::Type::Operator, .start = token_location, .source = finish() };
	}

	return Error {
		.details = errors::Unrecognized_Character { .invalid_character = peek() },
		.file = File_Range {
			.start = token_location,
			.stop  = token_location + 1
		}
	};
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
	ensure(last_rune_length != 0, "cannot rewind to not existing rune");
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

std::string quoted(std::string_view s)
{
	std::string r;
	r.reserve(s.size() + 2);
	r += '"';
	for (auto c : s) {
		switch (c) {
		break; case '\'': r += "\\\\";
		break; case '"':  r += "\\\"";
		break; case '\t': r += "\\t";
		break; case '\n': r += "\\n";
		break; case '\v': r += "\\v";
		break; case '\r': r += "\\r";
		break; default:   r += c;
		}
	}
	return r += '"';
}

std::ostream& operator<<(std::ostream& os, Token const& token)
{
	return os << '{' << token.type << ", " << quoted(token.source) << ", " << token.start << '}';
}

std::ostream& operator<<(std::ostream& os, Token::Type type)
{
	switch (type) {
	case Token::Type::Open_Paren:                   return os << "OPEN PAREN";
	case Token::Type::Chord:                 return os << "CHORD";
	case Token::Type::Close_Bracket:           return os << "CLOSE BRACKET";
	case Token::Type::Comma:                 return os << "COMMA";
	case Token::Type::Close_Paren:                   return os << "CLOSE PAREN";
	case Token::Type::Keyword:               return os << "KEYWORD";
	case Token::Type::Nl:                    return os << "NL";
	case Token::Type::Numeric:               return os << "NUMERIC";
	case Token::Type::Open_Bracket:            return os << "OPEN BRACKET";
	case Token::Type::Operator:              return os << "OPERATOR";
	case Token::Type::Parameter_Separator:   return os << "PARAMETER SEPARATOR";
	case Token::Type::Symbol:                return os << "SYMBOL";
	}
	unreachable();
}

std::string_view type_name(Token::Type type)
{
	switch (type) {
	case Token::Type::Open_Paren:                   return "(";
	case Token::Type::Chord:                 return "chord";
	case Token::Type::Close_Bracket:           return "]";
	case Token::Type::Comma:                 return ",";
	case Token::Type::Close_Paren:                   return ")";
	case Token::Type::Keyword:               return "keyword";
	case Token::Type::Nl:                    return "newline";
	case Token::Type::Numeric:               return "numeric";
	case Token::Type::Open_Bracket:            return "[";
	case Token::Type::Operator:              return "operator";
	case Token::Type::Parameter_Separator:   return "parameter separator";
	case Token::Type::Symbol:                return "symbol";
	}
	unreachable();
}

std::size_t std::hash<Token>::operator()(Token const& token) const
{
	return hash_combine(std::hash<std::string_view>{}(token.source), size_t(token.type));
}

bool Token::operator==(Token::Type type) const
{
	return this->type == type;
}
