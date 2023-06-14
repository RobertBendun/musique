#ifndef MUSIQUE_TOKEN_HH
#define MUSIQUE_TOKEN_HH

#include <musique/common.hh>
#include <musique/location.hh>

/// Lexical token representation for Musique language
struct Token
{
	/// Type of Token
	enum class Type : u16
	{
		Symbol,               ///< like repeat or choose or chord
		Keyword,              ///< like true, false, nil
		Operator,             ///< like "+", "-", "++", "<"
		Chord,                ///< chord or single note literal, like "c4"
		Numeric,              ///< numeric literal (floating point or integer)
		Parameter_Separator,  ///< "|" separaters arguments from block body
		Comma,                ///< "," separates expressions. Used mainly to separate calls, like `foo 1 2; bar 3 4`
		Nl,										///< "\n" seperates expressions similar to Comma
		Open_Paren,           ///< "(" starts anonymous block of code (potentially a function)
		Close_Paren,          ///< ")" ends anonymous block of code (potentially a function)
		Open_Bracket,         ///< "[" starts index section of index expression
		Close_Bracket,				///< "]" ends index section of index expression
	};

	/// Type of Keyword
	enum class Keyword : u16
	{
#define Keywords_Enumeration(X) \
		X(And) \
		X(Do) \
		X(Else) \
		X(Else_If) \
		X(End) \
		X(False) \
		X(For) \
		X(If) \
		X(Nil) \
		X(Or) \
		X(Then) \
		X(True) \
		X(While)

	#define X(KW) KW,
	Keywords_Enumeration(X)
	#undef X
	};

	/// Type of token
	Type type;

	/// Type of keyword (only valid when type == Token::Type::Keyword)
	Keyword keyword_type{};

	/// Offset in source file where token starts
	unsigned start;

	/// Matched source code to the token type
	std::string_view source;

	constexpr File_Range location(std::string_view filename) const
	{
		return {
			.filename = filename,
			.start = start,
			.stop = start + unsigned(source.size())
		};
	}

	bool operator==(Token::Type type) const;
};

static constexpr usize Operators_Count = 17;

std::string_view type_name(Token::Type type);

/// Token debug printing
std::ostream& operator<<(std::ostream& os, Token const& tok);

/// Token type debug printing
std::ostream& operator<<(std::ostream& os, Token::Type type);

template<> struct std::hash<Token>  { std::size_t operator()(Token  const&) const; };

#endif
