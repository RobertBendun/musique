#ifndef MUSIQUE_PARSER_HH
#define MUSIQUE_PARSER_HH

#include <musique/parser/ast.hh>
#include <musique/result.hh>

/// Source code to program tree converter
///
/// Intended to be used by library user only by Parser::parse() static function.
struct Parser
{
	/// List of tokens yielded from source
	std::vector<Token> tokens;

	/// Current token id (offset in tokens array)
	unsigned token_id = 0;

	std::string_view filename;

	/// Parses whole source code producing Ast or Error
	/// using Parser structure internally
	static Result<Ast> parse(std::string_view source, std::string_view filename, unsigned line_number = 0, bool print_tokens = false);

	/// Peek current token
	Result<Token> peek() const;

	/// Peek type of the current token
	Result<Token::Type> peek_type() const;

	/// Consume current token
	Token consume(Location const& location = Location::caller());

	template<typename ...Pattern>
	requires ((std::is_same_v<Pattern, Token::Type> || std::is_same_v<Pattern, std::pair<Token::Type, std::string_view>>) || ...)
	inline bool expect(Pattern const& ...pattern)
	{
		auto offset = 0u;
		auto const body = Overloaded {
			[&](Token::Type type) { return tokens[token_id + offset++].type == type; },
			[&](std::pair<Token::Type, std::string_view> pair) {
				auto [type, lexeme] = pair;
				auto &token = tokens[token_id + offset++];
				return token.type == type && token.source == lexeme;
			}
		};
		return token_id + sizeof...(Pattern) <= tokens.size() && (body(pattern) && ...);
	}
};

#endif
