#ifndef MUSIQUE_LEXER_HH
#define MUSIQUE_LEXER_HH

#include <musique/lexer/token.hh>
#include <musique/result.hh>
#include <variant>

/// Explicit marker of the end of file
struct End_Of_File {};

/// Lexer takes source code and turns it into list of tokens
///
/// It allows for creating sequence of tokens by using next_token() method.
/// On each call to next_token() when source is non empty token is lexed and
/// source is beeing advanced by removing matched token from string.
struct Lexer
{
	/// Source that is beeing lexed
	std::string_view source;

	/// Location in source of the last rune
	///
	/// Used only for rewinding
	std::uint32_t last_rune_length = 0;

	/// Start of the token that is currently beeing matched
	char const* token_start = nullptr;

	/// Bytes matched so far
	unsigned token_length = 0;

	/// Location of the start of a token that is currently beeing matched
	unsigned token_location{};

	/// Current location of Lexer in source
	unsigned location{};

	/// Previous location of Lexer in source
	///
	/// Used only for rewinding
	unsigned prev_location{};

	/// Try to tokenize next token.
	auto next_token() -> Result<std::variant<Token, End_Of_File>>;

	/// Skip whitespace and comments from the beggining of the source
	///
	/// Utility function for next_token()
	void skip_whitespace_and_comments();

	/// Finds next rune in source
	auto peek() const -> u32;

	/// Finds next rune in source and returns it, advancing the string
	auto consume() -> u32;

	/// For test beeing
	///	 callable,  current rune is passed to test
	///  integral,  current rune is tested for equality with test
	///  string,    current rune is tested for beeing in it
	///  otherwise, current rune is tested for beeing in test
	///
	///  When testing above yields truth, current rune is consumed.
	///  Returns if rune was consumed
	auto consume_if(auto test) -> bool;

	/// Consume two runes with given tests otherwise backtrack
	auto consume_if(auto first, auto second) -> bool;

	/// Goes back last rune
	void rewind();

	/// Marks begin of token
	void start();

	/// Marks end of token and returns it's matching source
	std::string_view finish();
};

#endif
