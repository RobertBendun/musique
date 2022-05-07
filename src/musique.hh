#pragma once

#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>
#include <tl/expected.hpp>
#include <variant>

#if defined(__cpp_lib_source_location)
#include <source_location>
#endif

// To make sure, that we don't collide with <cassert> macro
#ifdef assert
#undef assert
#endif

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
	usize line = 1, column = 1;

	Location advance(u32 rune);

	bool operator==(Location const& rhs) const = default;

	static Location at(usize line, usize column)
	{
		Location loc;
		loc.line = line;
		loc.column = column;
		return loc;
	}

	// Used to describe location of function call in interpreter (internal use only)
#if defined(__cpp_lib_source_location)
	static Location caller(std::source_location loc = std::source_location::current());
#elif (__has_builtin(__builtin_FILE) and __has_builtin(__builtin_LINE))
	static Location caller(char const* file = __builtin_FILE(), usize line = __builtin_LINE());
#else
#error Cannot implement Location::caller function
#endif
};

std::ostream& operator<<(std::ostream& os, Location const& location);

void assert(bool condition, std::string message, Location loc = Location::caller());

// Marks part of code that was not implemented yet
[[noreturn]] void unimplemented(Location loc = Location::caller());

// Marks location that should not be reached
[[noreturn]] void unreachable(Location loc = Location::caller());

struct Error
{
	errors::Type type;
	std::optional<Location> location = std::nullopt;
	std::string message{};

	bool operator==(errors::Type);
	Error with(Location) &&;
};

std::ostream& operator<<(std::ostream& os, Error const& err);

template<typename T>
struct Result : tl::expected<T, Error>
{
	using Storage = tl::expected<T, Error>;

	constexpr Result() = default;

	template<typename ...Args> requires (not std::is_void_v<T>) && std::is_constructible_v<T, Args...>
	constexpr Result(Args&& ...args)
		: Storage( T{ std::forward<Args>(args)... } )
	{
	}

	template<typename Arg> requires std::is_constructible_v<Storage, Arg>
	constexpr Result(Arg &&arg)
		: Storage(std::forward<Arg>(arg))
	{
	}

	constexpr Result(errors::Type error)
		: Storage(tl::unexpect, Error { error } )
	{
	}

	inline Result(Error error)
		: Storage(tl::unexpected(std::move(error)))
	{
	}

	// Internal function used for definition of Try macro
	inline auto value() &&
	{
		if constexpr (not std::is_void_v<T>) {
			return Storage::value();
		}
	}
};

// NOTE This implementation requires C++ language extension: statement expressions
// It's supported by GCC and Clang, other compilers i don't know
//
// Inspired by SerenityOS TRY macro
#define Try(Value)                               \
	({                                             \
		auto try_value = (Value);                    \
		if (not try_value.has_value()) [[unlikely]]  \
			return tl::unexpected(try_value.error());  \
		std::move(try_value).value();                \
	})

namespace unicode
{
	inline namespace special_runes
	{
		[[maybe_unused]] constexpr u32 Rune_Error = 0xfffd;
		[[maybe_unused]] constexpr u32 Rune_Self  = 0x80;
		[[maybe_unused]] constexpr u32 Max_Bytes  = 4;
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
	Result<void> ensure(Token::Type type) const;
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
