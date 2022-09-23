#ifndef MUSIQUE_VALUE_HH
#define MUSIQUE_VALUE_HH

#include <musique/value/array.hh>
#include <musique/value/block.hh>
#include <musique/value/chord.hh>
#include <musique/common.hh>
#include <musique/value/note.hh>
#include <musique/result.hh>
#include <musique/lexer/token.hh>

/// Representation of any value in language
struct Value
{
	/// Creates value from literal contained in Token
	static Result<Value> from(Token t);

	/// Create value holding provided boolean
	///
	/// Using Explicit_Bool to prevent from implicit casts
	static Value from(Explicit_Bool b);

	static Value from(Array &&array);              ///< Create value of type array holding provided array
	static Value from(Block &&l);                  ///< Create value of type block holding provided block
	static Value from(Chord chord);                ///< Create value of type music holding provided chord
	static Value from(Note n);                     ///< Create value of type music holding provided note
	static Value from(Number n);                   ///< Create value of type number holding provided number
	static Value from(char const* s);              ///< Create value of type symbol holding provided symbol
	static Value from(std::string s);              ///< Create value of type symbol holding provided symbol
	static Value from(std::string_view s);         ///< Create value of type symbol holding provided symbol
	static Value from(std::vector<Value> &&array); ///< Create value of type array holding provided array

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
	Result<Value> index(Interpreter &i, unsigned position) const;

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
template<> struct std::hash<Value>  { std::size_t operator()(Value  const&) const; };

/// Returns if type can be indexed
static constexpr bool is_indexable(Value::Type type)
{
	return type == Value::Type::Array || type == Value::Type::Block;
}

/// Returns if type can be called
static constexpr bool is_callable(Value::Type type)
{
	return type == Value::Type::Block || type == Value::Type::Intrinsic;
}


/// Binary operation may be vectorized when there are two argument which one is indexable and other is not
static inline bool may_be_vectorized(std::vector<Value> const& args)
{
	return args.size() == 2 && (is_indexable(args[0].type) != is_indexable(args[1].type));
}

template<typename T>
Result<Value> wrap_value(Result<T> &&value)
{
	return std::move(value).map([](auto &&value) { return Value::from(std::move(value)); });
}

Value wrap_value(auto &&value)
{
	return Value::from(std::move(value));
}

#endif
