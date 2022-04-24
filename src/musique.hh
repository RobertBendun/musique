#pragma once

#include <cstdint>
#include <string_view>
#include <ostream>
#include <tl/expected.hpp>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;


namespace errors
{
	enum Type
	{
		End_Of_File
	};
}

struct Error
{
	errors::Type type;
	Error *child = nullptr;

	bool operator==(errors::Type);
};

template<typename T>
using Result = tl::expected<T, Error>;

std::ostream& operator<<(std::ostream& os, Error const& err);

// NOTE This implementation requires C++ language extension: statement expressions
// It's supported by GCC, other compilers i don't know
#define Try(Value) ({ \
	auto try_value = (Value); \
	if (not try_value.has_value()) return tl::unexpected(try_value.error()); \
	*std::move(try_value); \
	})

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
};

std::ostream& operator<<(std::ostream& os, Token const& tok);

struct Lexer
{
	// Source that is beeing lexed
	std::string_view source;

	// Determine location of tokens to produce nice errors
	std::string_view source_name = "<unnamed>";
	unsigned column = 1, row = 1;

	auto next_token() -> Result<Token>;
};
