#ifndef MUSIQUE_TOKEN_HH
#define MUSIQUE_TOKEN_HH

#include "common.hh"
#include "location.hh"

/// Lexical token representation for Musique language
struct Token
{
	/// Type of Token
	enum class Type
	{
		Symbol,               ///< like repeat or choose or chord
		Keyword,              ///< like true, false, nil
		Operator,             ///< like "+", "-", "++", "<"
		Chord,                ///< chord or single note literal, like "c125"
		Numeric,              ///< numeric literal (floating point or integer)
		Parameter_Separator,  ///< "|" separaters arguments from block body
		Expression_Separator, ///< ";" separates expressions. Used mainly to separate calls, like `foo 1 2; bar 3 4`
		Open_Block,           ///< "[" delimits anonymous block of code (potentially a function)
		Close_Block,          ///< "]" delimits anonymous block of code (potentially a function)
		Open_Paren,           ///< "(" used in arithmetic or as function invocation sarrounding
		Close_Paren           ///< ")" used in arithmetic or as function invocation sarrounding
	};

	/// Type of token
	Type type;

	/// Matched source code to the token type
	std::string_view source;

	/// Location of encountered token
	Location location;
};

static constexpr usize Keywords_Count  =  5;
static constexpr usize Operators_Count = 17;

std::string_view type_name(Token::Type type);

/// Token debug printing
std::ostream& operator<<(std::ostream& os, Token const& tok);

/// Token type debug printing
std::ostream& operator<<(std::ostream& os, Token::Type type);

template<> struct std::hash<Token>  { std::size_t operator()(Token  const&) const; };

#endif
