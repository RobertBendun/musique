#pragma once

#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
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

using namespace std::string_literals;
using namespace std::string_view_literals;

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
		Failed_Numeric_Parsing,

		Function_Not_Defined,
		Unresolved_Operator,
		Expected_Keyword,
		Not_Callable
	};
}

/// \brief Location describes code position in `file line column` format.
///        It's used both to represent position in source files provided
//         to interpreter and internal interpreter usage.
struct Location
{
	std::string_view filename = "<unnamed>"; ///< File that location is pointing to
	usize line   = 1;                        ///< Line number (1 based) that location is pointing to
	usize column = 1;                        ///< Column number (1 based) that location is pointing to

	/// Advances line and column numbers based on provided rune
	///
	/// If rune is newline, then column is reset to 1, and line number is incremented.
	/// Otherwise column number is incremented.
	///
	/// @param rune Rune from which column and line numbers advancements are made.
	Location& advance(u32 rune);

	bool operator==(Location const& rhs) const = default;

	//! Creates location at default filename with specified line and column number
	static Location at(usize line, usize column);

	// Used to describe location of function call in interpreter (internal use only)
#if defined(__cpp_lib_source_location)
	static Location caller(std::source_location loc = std::source_location::current());
#elif (__has_builtin(__builtin_FILE) and __has_builtin(__builtin_LINE))
	static Location caller(char const* file = __builtin_FILE(), usize line = __builtin_LINE());
#else
#error Cannot implement Location::caller function
	/// Returns location of call in interpreter source code.
	///
	/// Example of reporting where `foo()` was beeing called:
	/// @code
	/// void foo(Location loc = Location::caller()) { std::cout << loc << '\n'; }
	/// @endcode
	static Location caller();
#endif
};

std::ostream& operator<<(std::ostream& os, Location const& location);

void assert(bool condition, std::string message, Location loc = Location::caller());

// Marks part of code that was not implemented yet
[[noreturn]] void unimplemented(std::string_view message = {}, Location loc = Location::caller());

// Marks location that should not be reached
[[noreturn]] void unreachable(Location loc = Location::caller());

struct Error
{
	errors::Type type;
	std::optional<Location> location = std::nullopt;
	std::string message{};
	std::errc error_code{};

	bool operator==(errors::Type);
	Error with(Location) &&;
};

std::ostream& operator<<(std::ostream& os, Error const& err);

template<template<typename ...> typename Template, typename>
struct is_template : std::false_type {};

template<template<typename ...> typename Template, typename ...T>
struct is_template<Template, Template<T...>> : std::true_type {};

template<template<typename ...> typename Template, typename T>
constexpr auto is_template_v = is_template<Template, T>::value;

template<typename T>
struct [[nodiscard("This value may contain critical error, so it should NOT be ignored")]] Result : tl::expected<T, Error>
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
			// NOTE This line in ideal world should be `return Storage::value()`
			// but C++ does not infer that this is rvalue context.
			// `std::add_rvalue_reference_t<Storage>::value()`
			// also does not work, so this is probably the best way to express this:
			return std::move(*static_cast<Storage*>(this)).value();
		}
	}

	inline tl::expected<T, Error> to_expected() &&
	{
		return *static_cast<Storage*>(this);
	}

	template<typename Map>
	requires is_template_v<Result, std::invoke_result_t<Map, T&&>>
	auto and_then(Map &&map) &&
	{
		return std::move(*static_cast<Storage*>(this)).and_then(
			[map = std::forward<Map>(map)](T &&value) {
				return std::move(map)(std::move(value)).to_expected();
			});
	}

	using Storage::and_then;
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

		// like true, false, nil
		Keyword,

		// like + - ++ < >
		Operator,

		// chord literal, like c125
		Chord,

		// numeric literal (floating point or integer)
		Numeric,

		// "|" separaters arguments from block body, and provides variable introduction syntax
		Parameter_Separator,

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
	static Ast binary(Token, Ast lhs, Ast rhs);
	static Ast block(Location location, Ast seq = sequence({}));
	static Ast call(std::vector<Ast> call);
	static Ast lambda(Location location, Ast seq = sequence({}), std::vector<Ast> parameters = {});
	static Ast literal(Token);
	static Ast sequence(std::vector<Ast> call);
	static Ast variable_declaration(Location loc, std::vector<Ast> lvalues, std::optional<Ast> rvalue);

	enum class Type
	{
		Binary,               // Binary operator application like `1` + `2`
		Block,                // Block expressions like `[42; hello]`
		Lambda,               // Block expression beeing functions like `[i|i+1]`
		Call,                 // Function call application like `print 42`
		Literal,              // Compile time known constant like `c` or `1`
		Sequence,             // Several expressions sequences like `42`, `42; 32`
		Variable_Declaration, // Declaration of a variable with optional value assigment like `var x = 10` or `var y`
	};

	Type type;
	Location location;
	Token token;
	std::vector<Ast> arguments{};
};

bool operator==(Ast const& lhs, Ast const& rhs);
std::ostream& operator<<(std::ostream& os, Ast::Type type);
std::ostream& operator<<(std::ostream& os, Ast const& tree);
void dump(Ast const& ast, unsigned indent = 0);

struct Parser
{
	std::vector<Token> tokens;
	unsigned token_id = 0;

	// Parses whole source code producing Ast or Error
	// using Parser structure internally
	static Result<Ast> parse(std::string_view source, std::string_view filename);

	Result<Ast> parse_sequence();
	Result<Ast> parse_expression();
	Result<Ast> parse_infix_expression();
	Result<Ast> parse_atomic_expression();
	Result<Ast> parse_variable_declaration();

	Result<Ast> parse_identifier_with_trailing_separators();
	Result<Ast> parse_identifier();

	Result<Token> peek() const;
	Result<Token::Type> peek_type() const;
	Token consume();

	// Tests if current token has given type
	bool expect(Token::Type type) const;
	bool expect(Token::Type type, std::string_view lexeme) const;

	// Ensures that current token has one of types given.
	// Otherwise returns error
	Result<void> ensure(Token::Type type) const;
};

// Number type supporting integer and fractional constants
// Invariant: gcd(num, den) == 1, after any operation
struct Number
{
	using value_type = i64;
	value_type num = 0, den = 1;

	constexpr Number()              = default;
	constexpr Number(Number const&) = default;
	constexpr Number(Number &&)     = default;
	constexpr Number& operator=(Number const&) = default;
	constexpr Number& operator=(Number &&)     = default;

	explicit Number(value_type v);
	Number(value_type num, value_type den);

	auto as_int()      const -> value_type; // Returns self as int
	auto simplify()    const -> Number;     // Returns self, but with gcd(num, den) == 1
	void simplify_inplace();                // Update self, to have gcd(num, den) == 1

	bool operator==(Number const&) const;
	bool operator!=(Number const&) const;
	std::strong_ordering operator<=>(Number const&) const;

	Number  operator+(Number const& rhs) const;
	Number& operator+=(Number const& rhs);
	Number  operator-(Number const& rhs) const;
	Number& operator-=(Number const& rhs);
	Number  operator*(Number const& rhs) const;
	Number& operator*=(Number const& rhs);
	Number  operator/(Number const& rhs) const;
	Number& operator/=(Number const& rhs);

	static Result<Number> from(Token token);
};

std::ostream& operator<<(std::ostream& os, Number const& num);

struct Env;
struct Interpreter;
struct Value;

using Intrinsic = Result<Value>(*)(Interpreter &i, std::vector<Value>);

struct Block
{
	Location location;
	std::vector<std::string> parameters;
	Ast body;
	std::shared_ptr<Env> context;

	Result<Value> operator()(Interpreter &i, std::vector<Value> params);
	Result<Value> index(Interpreter &i, unsigned position);
};

struct Note
{
	/// Base of a note, like `c` (=0), `c#` (=1) `d` (=2)
	u8 base;

	/// Octave in MIDI acceptable range (from -1 to 9 inclusive)
	std::optional<i8> octave = std::nullopt;

	/// Length of playing note
	std::optional<Number> length = std::nullopt;

	/// Create Note from string
	static std::optional<Note> from(std::string_view note);

	/// Extract midi note number
	std::optional<u8> into_midi_note() const;

	/// Extract midi note number, but when octave is not present use provided default
	u8 into_midi_note(i8 default_octave) const;
};

template<typename T, typename ...XS>
constexpr auto is_one_of = (std::is_same_v<T, XS> || ...);

// TODO Add location
struct Value
{
	static Result<Value> from(Token t);
	static Value boolean(bool b);
	static Value number(Number n);
	static Value symbol(std::string s);
	static Value block(Block &&l);

	enum class Type
	{
		Nil,
		Bool,
		Number,
		Symbol,
		Intrinsic,
		Block,
		Music
	};

	Value() = default;
	Value(Value const&) = default;
	Value(Value &&) = default;
	Value& operator=(Value const&) = default;
	Value& operator=(Value &&) = default;

	inline Value(Intrinsic intr) : type{Type::Intrinsic}, intr(intr)
	{
	}

	Type type = Type::Nil;
	bool b;
	Number n;
	Intrinsic intr;
	Block blk;
	Note note;

	// TODO Most strings should not be allocated by Value, but reference to string allocated previously
	// Wrapper for std::string is needed that will allocate only when needed, middle ground between:
	//   std::string      - always owning string type
	//   std::string_view - not-owning string type
	std::string s{};

	bool truthy() const;
	bool falsy() const;
	Result<Value> operator()(Interpreter &i, std::vector<Value> args);

	bool operator==(Value const& other) const;
};

std::string_view type_name(Value::Type t);

std::ostream& operator<<(std::ostream& os, Value const& v);

struct Env : std::enable_shared_from_this<Env>
{
	// Constructor of Env class
	static std::shared_ptr<Env> make();

	static std::shared_ptr<Env> global;
	std::unordered_map<std::string, Value> variables;
	std::shared_ptr<Env> parent;

	Env(Env const&) = delete;
	Env(Env &&) = default;
	Env& operator=(Env const&) = delete;
	Env& operator=(Env &&) = default;

	/// Defines new variable regardless of it's current existance
	Env& force_define(std::string name, Value new_value);
	Value* find(std::string const& name);

	// Scope menagment
	std::shared_ptr<Env> enter();
	std::shared_ptr<Env> leave();

private:
	// Ensure that all values of this class are behind shared_ptr
	Env() = default;
};

struct Interpreter
{
	/// Output of IO builtins like `say`
	std::ostream &out;

	/// Operators defined for language
	std::unordered_map<std::string, Intrinsic> operators;

	/// Current environment (current scope)
	std::shared_ptr<Env> env;

	Interpreter();
	~Interpreter();
	explicit Interpreter(std::ostream& out);
	Interpreter(Interpreter const&) = delete;
	Interpreter(Interpreter &&) = default;

	Result<Value> eval(Ast &&ast);

	// Scope managment
	void enter_scope();
	void leave_scope();
};

namespace errors
{
	Error unrecognized_character(u32 invalid_character);
	Error unrecognized_character(u32 invalid_character, Location location);

	Error unexpected_token(Token::Type expected, Token const& unexpected);
	Error unexpected_token(Token const& unexpected);
	Error unexpected_end_of_source(Location location);

	Error failed_numeric_parsing(Location location, std::errc errc, std::string_view source);

	Error function_not_defined(Value const& v);
	Error unresolved_operator(Token const& op);
	Error expected_keyword(Token const& unexpected, std::string_view keyword);

	Error not_callable(std::optional<Location> location, Value::Type value_type);

	[[noreturn]]
	void all_tokens_were_not_parsed(std::span<Token>);
}
