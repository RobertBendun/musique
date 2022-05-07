#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>
#include <tl/expected.hpp>
#include <variant>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

struct Unit {};

#define Fun(Function) ([]<typename ...T>(T&& ...args) { return (Function)(std::forward<T>(args)...); })

// Error handling mechanism inspired by Andrew Kelly approach, that was implemented
// as first class feature in Zig programming language.
namespace errors
{
	enum Type
	{
		End_Of_File,
		Unrecognized_Character,

		Unexpected_Token_Type,
		Unexpected_Empty_Source,
	};
}

struct Location
{
	std::string_view filename = "<unnamed>";
	usize column = 1, line = 1;

	Location advance(u32 rune);

	bool operator==(Location const& rhs) const = default;

	static Location at(usize line, usize column)
	{
		Location loc;
		loc.line = line;
		loc.column = column;
		return loc;
	}
};

std::ostream& operator<<(std::ostream& os, Location const& location);

struct Error
{
	errors::Type type;
	std::optional<Location> location = std::nullopt;
	std::string message{};

	bool operator==(errors::Type);
	Error with(Location) &&;
};

template<typename T>
struct Result : tl::expected<T, Error>
{
	using Storage = tl::expected<T, Error>;

	constexpr Result()                         = default;
	constexpr Result(Result const&)            = default;
	constexpr Result(Result&&)                 = default;
	constexpr Result& operator=(Result const&) = default;
	constexpr Result& operator=(Result&&)      = default;

	constexpr Result(errors::Type error)
		: Storage(tl::unexpected(Error { error }))
	{
	}

	inline Result(Error error)
		: Storage(tl::unexpected(std::move(error)))
	{
	}

	inline Result(tl::unexpected<Error> error)
		: Storage(std::move(error))
	{
	}

	template<typename ...Args>
	requires std::is_constructible_v<T, Args...>
	constexpr Result(Args&& ...args)
		: Storage( T{ std::forward<Args>(args)... } )
	{
	}
};

std::ostream& operator<<(std::ostream& os, Error const& err);

// NOTE This implementation requires C++ language extension: statement expressions
// It's supported by GCC, other compilers i don't know
#define Try(Value) ({ \
	auto try_value = (Value); \
	if (not try_value.has_value()) return tl::unexpected(try_value.error()); \
	*std::move(try_value); \
	})

namespace unicode
{
	inline namespace special_runes
	{
		constexpr u32 Rune_Error = 0xfffd;
		constexpr u32 Rune_Self  = 0x80;
		constexpr u32 Max_Bytes  = 4;
	}

	// is_digit returns true if `digit` is ASCII digit
	bool is_digit(u32 digit);

	// is_space return true if `space` is ASCII blank character
	bool is_space(u32 space);

	// is_letter returns true if `letter` is considered a letter by Unicode
	bool is_letter(u32 letter);

	// is_identifier returns true if `letter` is valid character for identifier.
	//
	// It's modifier by is_first_character flag to determine some character classes
	// allowance like numbers, which are only allowed NOT at the front of the identifier
	enum class First_Character : bool { Yes = true, No = false };
	bool is_identifier(u32 letter, First_Character is_first_character);
}

namespace utf8
{
	using namespace unicode::special_runes;

	// Decodes rune and returns remaining string
	auto decode(std::string_view s) -> std::pair<u32, std::string_view>;
	auto length(std::string_view s) -> usize;

	struct Print { u32 rune; };
}

std::ostream& operator<<(std::ostream& os, utf8::Print const& print);

struct Token
{
	enum class Type
	{
		// like repeat or choose or chord
		Symbol,

		// like + - ++ < >
		Operator,

		// chord literal, like c125
		Chord,

		// numeric literal (floating point or integer)
		Numeric,

		// "|" separaters arguments from block body, and provides variable introduction syntax
		Variable_Separator,

		// ";" separates expressions. Used to separate calls, like `foo 1 2; bar 3 4`
		Expression_Separator,

		// "[" and "]", delimit anonymous block of code (potentially a function)
		Open_Block,
		Close_Block,

		// "(" and ")", used in arithmetic or as function invocation sarrounding (like in Haskell)
		Open_Paren,
		Close_Paren
	};

	Type type;
	std::string_view source;
	Location location;
};

std::ostream& operator<<(std::ostream& os, Token const& tok);
std::ostream& operator<<(std::ostream& os, Token::Type type);

struct Lexer
{
	// Source that is beeing lexed
	std::string_view source;

	// Used for rewinding
	u32 last_rune_length = 0;

	char const* token_start = nullptr;
	usize token_length = 0;
	Location token_location{};

	Location prev_location{};
	Location location{};

	auto next_token() -> Result<Token>;

	// Utility function for next_token()
	void skip_whitespace_and_comments();

	// Finds next rune in source
	auto peek() const -> u32;

	// Finds next rune in source and returns it, advancing the string
	auto consume() -> u32;

	// For test beeing
	//	callable,  current rune is passed to test
	//  integral,  current rune is tested for equality with test
	//  string,    current rune is tested for beeing in it
	//  otherwise, current rune is tested for beeing in test
	//
	//  When testing above yields truth, current rune is consumed.
	//  Returns if rune was consumed
	auto consume_if(auto test) -> bool;

	// Consume two runes with given tests otherwise backtrack
	auto consume_if(auto first, auto second) -> bool;

	// Goes back last rune
	void rewind();

	// Marks begin of token
	void start();

	// Marks end of token and returns it's matching source
	std::string_view finish();
};

struct Ast
{
	// Named constructors of AST structure
	static Ast literal(Token);

	enum class Type
	{
		Literal
	};

	Type type;
	Token token;
};

template<typename Expected, typename ...T>
concept Var_Args = (std::is_same_v<Expected, T> && ...) && (sizeof...(T) >= 1);

struct Parser
{
	std::vector<Token> tokens;
	unsigned token_id = 0;

	// Parses whole source code producing Ast or Error
	// using Parser structure internally
	static Result<Ast> parse(std::string_view source, std::string_view filename);

	Result<Ast> parse_expression();
	Result<Ast> parse_binary_operator();
	Result<Ast> parse_literal();

	Token consume();

	// Tests if current token has given type
	bool expect(Token::Type type) const;

	// Ensures that current token has one of types given.
	// Otherwise returns error
	Result<Unit> ensure(Token::Type type) const;
};

namespace errors
{
	Error unrecognized_character(u32 invalid_character);
	Error unrecognized_character(u32 invalid_character, Location location);

	Error unexpected_token(Token::Type expected, Token const& unexpected);
	Error unexpected_end_of_source(Location location);

	[[noreturn]]
	void all_tokens_were_not_parsed(std::span<Token>);
}
