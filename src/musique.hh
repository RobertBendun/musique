#pragma once

#include <cassert>
#include <cstdint>
#include <optional>
#include <ostream>
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

#define Fun(Function) ([]<typename ...T>(T&& ...args) { return (Function)(std::forward<T>(args)...); })

namespace errors
{
	enum Type
	{
		End_Of_File,
		Unrecognized_Character
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
	u32 invalid_character = 0;

	bool operator==(errors::Type);
};

template<typename T>
struct Result : tl::expected<T, Error>
{
	constexpr Result() = default;

	constexpr Result(errors::Type error) : tl::expected<T, Error>(tl::unexpected(Error { error }))
	{
	}

	template<typename ...Args>
	constexpr Result(Args&& ...args)
		: tl::expected<T, Error>( T{ std::forward<Args>(args)... } )
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

	bool is_digit(u32 digit);
	bool is_space(u32 space);
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

		// chord literal, like c125
		Chord,

		// numeric literal (floating point or integer)
		Numeric,

		// "|" separaters arguments from block body, and provides variable introduction syntax
		Variable_Separator,

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

	// Finds next rune in source
	auto peek() const -> u32;

	// Finds next rune in source and returns it, advancing the string
	auto consume() -> u32;

	inline auto consume_if(auto test) -> u32
	{
		return test(peek()) && (consume(), true);
	}

	// Goes back last rune
	void rewind();

	// Marks begin of token
	void start();

	// Marks end of token and returns it's matching source
	std::string_view finish();
};
