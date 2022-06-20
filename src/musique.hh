#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string_view>
#include <variant>

#include <midi.hh>
#include <tl/expected.hpp>

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

/// Error handling related functions and definitions
namespace errors
{
	/// When user puts emoji in the source code
	struct Unrecognized_Character
	{
		u32 invalid_character;
	};

	/// When parser was expecting code but encountered end of file
	struct Unexpected_Empty_Source
	{
	};

	/// When user passed numeric literal too big for numeric type
	struct Failed_Numeric_Parsing
	{
		std::errc reason;
	};

	/// When user forgot semicolon or brackets
	struct Expected_Expression_Separator_Before
	{
		std::string_view what;
	};

	/// When some keywords are not allowed in given context
	struct Unexpected_Keyword
	{
		std::string_view keyword;
	};

	/// When user tried to use operator that was not defined
	struct Undefined_Operator
	{
		std::string_view op;
	};

	/// When user tried to call something that can't be called
	struct Not_Callable
	{
		std::string_view type;
	};

	/// When user provides literal where identifier should be
	struct Literal_As_Identifier
	{
		std::string_view type_name;
		std::string_view source;
		std::string_view context;
	};

	/// When user provides wrong type for given operation
	struct Unsupported_Types_For
	{
		/// Type of operation
		enum { Operator, Function } type;

		/// Name of operation
		std::string_view name;

		/// Possible ways to use it correctly
		std::vector<std::string> possibilities;
	};

	/// Collection of messages that are considered internal and should not be printed to the end user.
	namespace internal
	{
		/// When encountered token that was supposed to be matched in higher branch of the parser
		struct Unexpected_Token
		{
			/// Type of the token
			std::string_view type;

			/// Source of the token
			std::string_view source;

			/// Where this token was encountered that was unexpected?
			std::string_view when;
		};
	}

	/// All possible error types
	using Details = std::variant<
		Expected_Expression_Separator_Before,
		Failed_Numeric_Parsing,
		Literal_As_Identifier,
		Not_Callable,
		Undefined_Operator,
		Unexpected_Empty_Source,
		Unexpected_Keyword,
		Unrecognized_Character,
		Unsupported_Types_For,
		internal::Unexpected_Token
	>;
}

/// All code related to pretty printing. Default mode is no_color
namespace pretty
{
	/// Mark start of printing an error
	std::ostream& begin_error(std::ostream&);

	/// Mark start of printing a path
	std::ostream& begin_path(std::ostream&);

	/// Mark start of printing a comment
	std::ostream& begin_comment(std::ostream&);

	/// Mark end of any above
	std::ostream& end(std::ostream&);

	/// Switch to colorful output via ANSI escape sequences
	void terminal_mode();

	/// Switch to colorless output (default one)
	void no_color_mode();
}

/// Combine several lambdas into one for visiting std::variant
template<typename ...Lambdas>
struct Overloaded : Lambdas... { using Lambdas::operator()...; };

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

/// Guards that program exits if condition does not hold
void assert(bool condition, std::string message, Location loc = Location::caller());

/// Marks part of code that was not implemented yet
[[noreturn]] void unimplemented(std::string_view message = {}, Location loc = Location::caller());

/// Marks location that should not be reached
[[noreturn]] void unreachable(Location loc = Location::caller());

/// Represents all recoverable error messages that interpreter can produce
struct Error
{
	/// Specific message details
	errors::Details details;

	/// Location that coused all this trouble
	std::optional<Location> location = std::nullopt;

	/// Return self with new location
	Error with(Location) &&;
};

/// Error pretty printing
std::ostream& operator<<(std::ostream& os, Error const& err);

/// Returns if provided thingy is a given template
template<template<typename ...> typename Template, typename>
struct is_template : std::false_type {};

template<template<typename ...> typename Template, typename ...T>
struct is_template<Template, Template<T...>> : std::true_type {};

/// Returns if provided thingy is a given template
template<template<typename ...> typename Template, typename T>
constexpr auto is_template_v = is_template<Template, T>::value;

/// Holds either T or Error
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

/// Shorthand for forwarding error values with Result type family.
///
/// This implementation requires C++ language extension: statement expressions
/// It's supported by GCC and Clang, other compilers i don't know.
/// Inspired by SerenityOS TRY macro
#define Try(Value)                               \
	({                                             \
		auto try_value = (Value);                    \
		if (not try_value.has_value()) [[unlikely]]  \
			return tl::unexpected(try_value.error());  \
		std::move(try_value).value();                \
	})

/// Drop in replacement for bool when C++ implcit conversions stand in your way
struct Explicit_Bool
{
	bool value;

	constexpr Explicit_Bool(bool b) : value(b)
	{
	}

	constexpr Explicit_Bool(auto &&) = delete;

	constexpr operator bool() const
	{
		return value;
	}
};

/// All unicode related operations
namespace unicode
{
	inline namespace special_runes
	{
		[[maybe_unused]] constexpr u32 Rune_Error = 0xfffd;
		[[maybe_unused]] constexpr u32 Rune_Self  = 0x80;
		[[maybe_unused]] constexpr u32 Max_Bytes  = 4;
	}

	/// is_digit returns true if `digit` is ASCII digit
	bool is_digit(u32 digit);

	/// is_space return true if `space` is ASCII blank character
	bool is_space(u32 space);

	/// is_letter returns true if `letter` is considered a letter by Unicode
	bool is_letter(u32 letter);

	/// is_identifier returns true if `letter` is valid character for identifier.
	///
	/// It's modifier by is_first_character flag to determine some character classes
	/// allowance like numbers, which are only allowed NOT at the front of the identifier
	enum class First_Character : bool { Yes = true, No = false };
	bool is_identifier(u32 letter, First_Character is_first_character);
}

/// utf8 encoding and decoding
namespace utf8
{
	using namespace unicode::special_runes;

	/// Decodes rune and returns remaining string
	auto decode(std::string_view s) -> std::pair<u32, std::string_view>;

	/// Returns length of the first rune in the provided string
	auto length(std::string_view s) -> usize;

	struct Print { u32 rune; };
}

std::ostream& operator<<(std::ostream& os, utf8::Print const& print);

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

static constexpr auto Keywords_Count = 6;

std::string_view type_name(Token::Type type);

/// Token debug printing
std::ostream& operator<<(std::ostream& os, Token const& tok);

/// Token type debug printing
std::ostream& operator<<(std::ostream& os, Token::Type type);

struct Lines
{
	static Lines the;

	/// Region of lines in files
	std::unordered_map<std::string, std::vector<std::string_view>> lines;

	/// Add lines from file
	void add_file(std::string filename, std::string_view source);

	/// Add single line into file (REPL usage)
	void add_line(std::string const& filename, std::string_view source);

	/// Print selected region
	void print(std::ostream& os, std::string const& file, unsigned first_line, unsigned last_line) const;
};

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
	u32 last_rune_length = 0;

	/// Start of the token that is currently beeing matched
	char const* token_start = nullptr;

	/// Bytes matched so far
	usize token_length = 0;

	/// Location of the start of a token that is currently beeing matched
	Location token_location{};

	/// Current location of Lexer in source
	Location location{};

	/// Previous location of Lexer in source
	///
	/// Used only for rewinding
	Location prev_location{};

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

/// Representation of a node in program tree
struct Ast
{
	/// Constructs binary operator
	static Ast binary(Token, Ast lhs, Ast rhs);

	/// Constructs block
	static Ast block(Location location, Ast seq = sequence({}));

	/// Constructs call expression
	static Ast call(std::vector<Ast> call);

	/// Constructs block with parameters
	static Ast lambda(Location location, Ast seq = sequence({}), std::vector<Ast> parameters = {});

	/// Constructs constants, literals and variable identifiers
	static Ast literal(Token);

	/// Constructs sequence of operations
	static Ast sequence(std::vector<Ast> call);

	/// Constructs variable declaration
	static Ast variable_declaration(Location loc, std::vector<Ast> lvalues, std::optional<Ast> rvalue);

	/// Available ASt types
	enum class Type
	{
		Binary,               ///< Binary operator application like `1` + `2`
		Block,                ///< Block expressions like `[42; hello]`
		Lambda,               ///< Block expression beeing functions like `[i|i+1]`
		Call,                 ///< Function call application like `print 42`
		Literal,              ///< Compile time known constant like `c` or `1`
		Sequence,             ///< Several expressions sequences like `42`, `42; 32`
		Variable_Declaration, ///< Declaration of a variable with optional value assigment like `var x = 10` or `var y`
	};

	/// Type of AST node
	Type type;

	/// Location that introduced this node
	Location location;

	/// Associated token
	Token token;

	/// Child nodes
	std::vector<Ast> arguments{};
};

bool operator==(Ast const& lhs, Ast const& rhs);
std::ostream& operator<<(std::ostream& os, Ast::Type type);
std::ostream& operator<<(std::ostream& os, Ast const& tree);

/// Pretty print program tree for debugging purposes
void dump(Ast const& ast, unsigned indent = 0);

/// Source code to program tree converter
///
/// Intended to be used by library user only by Parser::parse() static function.
struct Parser
{
	/// List of tokens yielded from source
	std::vector<Token> tokens;

	/// Current token id (offset in tokens array)
	unsigned token_id = 0;

	/// Parses whole source code producing Ast or Error
	/// using Parser structure internally
	static Result<Ast> parse(std::string_view source, std::string_view filename);

	/// Parse sequence, collection of expressions
	Result<Ast> parse_sequence();

	/// Parse either infix expression or variable declaration
	Result<Ast> parse_expression();

	/// Parse infix expression
	Result<Ast> parse_infix_expression();

	/// Parse right hand size of infix expression
	Result<Ast> parse_rhs_of_infix_expression(Ast lhs);

	/// Parse either index expression or atomic expression
	Result<Ast> parse_index_expression();

	/// Parse function call, literal etc
	Result<Ast> parse_atomic_expression();

	/// Parse variable declaration
	Result<Ast> parse_variable_declaration();

	/// Utility function for identifier parsing
	Result<Ast> parse_identifier_with_trailing_separators();

	/// Utility function for identifier parsing
	Result<Ast> parse_identifier();

	/// Peek current token
	Result<Token> peek() const;

	/// Peek type of the current token
	Result<Token::Type> peek_type() const;

	/// Consume current token
	Token consume();

	/// Tests if current token has given type
	bool expect(Token::Type type) const;

	/// Tests if current token has given type and source
	bool expect(Token::Type type, std::string_view lexeme) const;
};

/// Number type supporting integer and fractional constants
///
/// \invariant gcd(num, den) == 1, after any operation
struct Number
{
	/// Type that represents numerator and denominator values
	using value_type = i64;

	value_type num = 0; ///< Numerator of a fraction beeing represented
	value_type den = 1; ///< Denominator of a fraction beeing represented

	constexpr Number()              = default;
	constexpr Number(Number const&) = default;
	constexpr Number(Number &&)     = default;
	constexpr Number& operator=(Number const&) = default;
	constexpr Number& operator=(Number &&)     = default;

	explicit Number(value_type v);          ///< Creates Number as fraction v / 1
	Number(value_type num, value_type den); ///< Creates Number as fraction num / den

	auto as_int()      const -> value_type; ///< Returns self as int
	auto simplify()    const -> Number;     ///< Returns self, but with gcd(num, den) == 1
	void simplify_inplace();                ///< Update self, to have gcd(num, den) == 1

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

	/// Parses source contained by token into a Number instance
	static Result<Number> from(Token token);
};

std::ostream& operator<<(std::ostream& os, Number const& num);

struct Env;
struct Interpreter;
struct Value;

using Intrinsic = Result<Value>(*)(Interpreter &i, std::vector<Value>);

/// Lazy Array / Continuation / Closure type thingy
struct Block
{
	/// Location of definition / creation
	Location location;

	/// Names of expected parameters
	std::vector<std::string> parameters;

	/// Body that will be executed
	Ast body;

	/// Context from which block was created. Used for closures
	std::shared_ptr<Env> context;

	/// Calling block
	Result<Value> operator()(Interpreter &i, std::vector<Value> params);

	/// Indexing block
	Result<Value> index(Interpreter &i, unsigned position);

	/// Count of elements in block
	usize size() const;
};

/// Representation of musical note
struct Note
{
	/// Base of a note, like `c` (=0), `c#` (=1) `d` (=2)
	i32 base;

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

	bool operator==(Note const&) const;

	std::partial_ordering operator<=>(Note const&) const;

	/// Simplify note by adding base to octave if octave is present
	void simplify_inplace();
};

std::ostream& operator<<(std::ostream& os, Note note);

/// Represantation of simultaneously played notes, aka musical chord
struct Chord
{
	std::vector<Note> notes; ///< Notes composing a chord

	/// Parse chord literal from provided source
	static Chord from(std::string_view source);

	bool operator==(Chord const&) const = default;
};

std::ostream& operator<<(std::ostream& os, Chord const& chord);

/// Eager Array
struct Array
{
	/// Elements that are stored in array
	std::vector<Value> elements;

	/// Index element of an array
	Result<Value> index(Interpreter &i, unsigned position);

	/// Count of elements
	usize size() const;

	bool operator==(Array const&) const = default;
};

std::ostream& operator<<(std::ostream& os, Array const& v);

/// Representation of any value in language
struct Value
{
	/// Creates value from literal contained in Token
	static Result<Value> from(Token t);

	/// Create value holding provided boolean
	///
	/// Using Explicit_Bool to prevent from implicit casts
	static Value from(Explicit_Bool b);

	static Value from(Number n);           ///< Create value of type number holding provided number
	static Value from(std::string s);      ///< Create value of type symbol holding provided symbol
	static Value from(std::string_view s); ///< Create value of type symbol holding provided symbol
	static Value from(char const* s);      ///< Create value of type symbol holding provided symbol
	static Value from(Block &&l);          ///< Create value of type block holding provided block
	static Value from(Array &&array);      ///< Create value of type array holding provided array
	static Value from(Note n);             ///< Create value of type music holding provided note
	static Value from(Chord chord);        ///< Create value of type music holding provided chord

	enum class Type
	{
		Nil,        ///< Unit type, used for denoting emptiness and result of some side effect only functions
		Bool,       ///< Boolean type, used for logic computations
		Number,     ///< Number type, representing only rational numbers
		Symbol,     ///< Symbol type, used to represent identifiers
		Intrinsic,  ///< Intrinsic functions that are implemented in C++
		Block,      ///< Block type, containing block value (lazy array/closure/lambda like)
		Array,      ///< Array type, eager array
		Music,      ///< Music type,
	};

	Value() = default;
	Value(Value const&) = default;
	Value(Value &&) = default;
	Value& operator=(Value const&) = default;
	Value& operator=(Value &&) = default;

	/// Contructs Intrinsic, used to simplify definition of intrinsics
	inline Value(Intrinsic intr) : type{Type::Intrinsic}, intr(intr)
	{
	}

	Type type = Type::Nil;
	bool b;
	Number n;
	Intrinsic intr;
	Block blk;
	Chord chord;
	Array array;

	// TODO Most strings should not be allocated by Value, but reference to string allocated previously
	// Wrapper for std::string is needed that will allocate only when needed, middle ground between:
	//   std::string      - always owning string type
	//   std::string_view - not-owning string type
	std::string s{};

	/// Returns truth judgment for current type, used primarly for if function
	bool truthy() const;

	/// Returns false judgment for current type, used primarly for if function
	bool falsy() const;

	/// Calls contained value if it can be called
	Result<Value> operator()(Interpreter &i, std::vector<Value> args);

	/// Index contained value if it can be called
	Result<Value> index(Interpreter &i, unsigned position);

	/// Return elements count of contained value if it can be measured
	usize size() const;

	bool operator==(Value const& other) const;

	std::partial_ordering operator<=>(Value const& other) const;
};

template<Value::Type>
struct Member_For_Value_Type {};

template<> struct Member_For_Value_Type<Value::Type::Bool>
{ static constexpr auto value = &Value::b; };

template<> struct Member_For_Value_Type<Value::Type::Number>
{ static constexpr auto value = &Value::n; };

template<> struct Member_For_Value_Type<Value::Type::Symbol>
{ static constexpr auto value = &Value::s; };

template<> struct Member_For_Value_Type<Value::Type::Intrinsic>
{ static constexpr auto value = &Value::intr; };

template<> struct Member_For_Value_Type<Value::Type::Block>
{ static constexpr auto value = &Value::blk; };

template<> struct Member_For_Value_Type<Value::Type::Array>
{ static constexpr auto value = &Value::array; };

template<> struct Member_For_Value_Type<Value::Type::Music>
{ static constexpr auto value = &Value::chord; };

/// Returns type name of Value type
std::string_view type_name(Value::Type t);

std::ostream& operator<<(std::ostream& os, Value const& v);

/// Collection holding all variables in given scope.
struct Env : std::enable_shared_from_this<Env>
{
	/// Constructor of Env class
	static std::shared_ptr<Env> make();

	/// Global scope that is beeing set by Interpreter
	static std::shared_ptr<Env> global;

	/// Variables in current scope
	std::unordered_map<std::string, Value> variables;

	/// Parent scope
	std::shared_ptr<Env> parent;

	Env(Env const&) = delete;
	Env(Env &&) = default;
	Env& operator=(Env const&) = delete;
	Env& operator=(Env &&) = default;

	/// Defines new variable regardless of it's current existance
	Env& force_define(std::string name, Value new_value);

	/// Finds variable in current or parent scopes
	Value* find(std::string const& name);

	/// Create new scope with self as parent
	std::shared_ptr<Env> enter();

	/// Leave current scope returning parent
	std::shared_ptr<Env> leave();

private:
	/// Ensure that all values of this class are behind shared_ptr
	Env() = default;
};

/// Context holds default values for music related actions
struct Context
{
	/// Default note octave
	i8 octave = 4;

	/// Default note length
	Number length = Number(1, 4);

	/// Default BPM
	unsigned bpm = 120;

	/// Fills empty places in Note like octave and length with default values from context
	Note fill(Note) const;

	/// Converts length to seconds with current bpm
	std::chrono::duration<float> length_to_duration(std::optional<Number> length) const;
};

/// Given program tree evaluates it into Value
struct Interpreter
{
	/// MIDI connection that is used to play music.
	/// It's optional for simple interpreter testing.
	midi::Connection *midi_connection = nullptr;

	/// Operators defined for language
	std::unordered_map<std::string, Intrinsic> operators;

	/// Current environment (current scope)
	std::shared_ptr<Env> env;

	/// Context stack. `constext_stack.back()` is a current context.
	/// There is always at least one context
	std::vector<Context> context_stack;

	struct Incoming_Midi_Callbacks;
	std::unique_ptr<Incoming_Midi_Callbacks> callbacks;
	void register_callbacks();

	Interpreter();
	~Interpreter();
	Interpreter(Interpreter const&) = delete;
	Interpreter(Interpreter &&) = default;

	/// Try to evaluate given program tree
	Result<Value> eval(Ast &&ast);

	// Enter scope by changing current environment
	void enter_scope();

	// Leave scope by changing current environment
	void leave_scope();

	/// Play note resolving any missing parameters with context via `midi_connection` member.
	void play(Chord);
};

namespace errors
{
	[[noreturn]]
	void all_tokens_were_not_parsed(std::span<Token>);
}
