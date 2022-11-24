#ifndef MUSIQUE_VALUE_HH
#define MUSIQUE_VALUE_HH

// Needs to be always first, before all the implicit template instantiations
#include <musique/value/hash.hh>

#include <musique/accessors.hh>
#include <musique/common.hh>
#include <musique/lexer/token.hh>
#include <musique/result.hh>
#include <musique/value/array.hh>
#include <musique/value/block.hh>
#include <musique/value/chord.hh>
#include <musique/value/intrinsic.hh>
#include <musique/value/note.hh>
#include <musique/value/set.hh>

struct Nil
{
	bool operator==(Nil const&) const = default;
};

using Bool   = bool;
using Symbol = std::string;

using Macro = Result<Value>(*)(Interpreter &i, std::span<Ast>);

/// Representation of any value in language
struct Value
{
	/// Creates value from literal contained in Token
	static Result<Value> from(Token t);

	/// Create value holding provided boolean
	///
	/// Using Explicit_Bool to prevent from implicit casts
	Value(Explicit_Bool b);

	Value(Array &&array);              ///< Create value of type array holding provided array
	Value(Block &&l);                  ///< Create value of type block holding provided block
	Value(Chord chord);                ///< Create value of type music holding provided chord
	Value(Note n);                     ///< Create value of type music holding provided note
	Value(Number n);                   ///< Create value of type number holding provided number
	Value(char const* s);              ///< Create value of type symbol holding provided symbol
	Value(std::string s);              ///< Create value of type symbol holding provided symbol
	Value(std::string_view s);         ///< Create value of type symbol holding provided symbol
	Value(Set &&set);                  ///< Create value of type set holding provided set
	explicit Value(std::vector<Value> &&array); ///< Create value of type array holding provided array

	// TODO Most strings should not be allocated by Value, but reference to string allocated previously
	// Wrapper for std::string is needed that will allocate only when needed, middle ground between:
	//   std::string      - always owning string type
	//   std::string_view - not-owning string type
	std::variant<
		Nil,
		Bool,
		Number,
		Symbol,
		Intrinsic,
		Block,
		Array,
		Set,
		Chord,
		Macro
	> data = Nil{};

	Value();
	Value(Value const&) = default;
	Value(Value &&) = default;
	Value& operator=(Value const&) = default;
	Value& operator=(Value &&) = default;
	~Value() = default;

	/// Contructs Intrinsic, used to simplify definition of intrinsics
	inline Value(Intrinsic::Function_Pointer intr) : data(Intrinsic(intr))
	{
	}

	inline Value(Intrinsic const& intr) : data(intr)
	{
	}

	inline Value(Macro m) : data(m)
	{
	}

	/// Returns truth judgment for current type, used primarly for if function
	bool truthy() const;

	/// Returns false judgment for current type, used primarly for if function
	bool falsy() const;

	/// Calls contained value if it can be called
	Result<Value> operator()(Interpreter &i, std::vector<Value> args) const;

	/// Index contained value if it can be called
	Result<Value> index(Interpreter &i, unsigned position) const;

	/// Return elements count of contained value if it can be measured
	usize size() const;

	bool operator==(Value const& other) const;

	std::partial_ordering operator<=>(Value const& other) const;
};

/// Forward variant operations to variant member
template<typename T>
inline T const* get_if(Value const& v) { return get_if<T const>(v.data); }

template<typename T>
inline T* get_if(Value& v) { return get_if<T>(v.data); }

/// Returns type name of Value type
std::string_view type_name(Value const& v);

template<typename T>
Result<Value> wrap_value(Result<T> &&value)
{
	return std::move(value).map([](auto &&value) { return Value(std::move(value)); });
}

template<typename Desired>
constexpr Desired& get_ref(Value &v)
{
	if constexpr (std::is_same_v<Desired, Value>) {
		return v;
	} else {
		if (auto result = get_if<Desired>(v)) { return *result; }
		unreachable();
	}
}

template<typename Desired>
constexpr bool holds_alternative(Value const& v)
{
	if constexpr (std::is_same_v<Desired, Value>) {
		return true;
	} else {
		return get_if<Desired>(v.data) != nullptr;
	}
}

template<typename Values>
concept With_Index_Operator = requires (Values &values, size_t i) {
	{ values[i] } -> std::convertible_to<Value>;
	{ values.size() } -> std::convertible_to<size_t>;
};

template<typename ...T>
constexpr auto match(With_Index_Operator auto& values) -> std::optional<std::tuple<T&...>>
{
	return [&]<std::size_t ...I>(std::index_sequence<I...>) -> std::optional<std::tuple<T&...>> {
		if (sizeof...(T) == values.size() && (holds_alternative<T>(values[I]) && ...)) {
			return {{ get_ref<T>(values[I])... }};
		} else {
			return std::nullopt;
		}
	} (std::make_index_sequence<sizeof...(T)>{});
}

template<typename ...T, typename ...Values>
constexpr auto match(Values& ...values) -> std::optional<std::tuple<T&...>>
{
	static_assert(sizeof...(T) == sizeof...(Values), "Provided parameters and expected types list must have the same length");

	return [&]<std::size_t ...I>(std::index_sequence<I...>) -> std::optional<std::tuple<T&...>> {
		if ((holds_alternative<T>(values) && ...)) {
			return {{ get_ref<T>(values)... }};
		} else {
			return std::nullopt;
		}
	} (std::make_index_sequence<sizeof...(T)>{});
}

std::ostream& operator<<(std::ostream& os, Value const& v);

#endif
