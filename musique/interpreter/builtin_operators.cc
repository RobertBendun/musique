#include <functional>
#include <musique/algo.hh>
#include <musique/guard.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>
#include <musique/value/intrinsic.hh>

#include <unordered_map>

/// Intrinsic implementation primitive to ease operation vectorization
static Result<Value> vectorize(auto &&operation, Interpreter &interpreter, Value lhs, Value rhs)
{
	auto lhs_coll = get_if<Collection>(lhs);
	auto rhs_coll = get_if<Collection>(rhs);

	if (lhs_coll != nullptr && rhs_coll == nullptr) {
		Array array;
		for (auto i = 0u; i < lhs_coll->size(); ++i) {
			array.elements.push_back(
				Try(operation(interpreter, { Try(lhs_coll->index(interpreter, i)), rhs })));
		}
		return array;
	}

	ensure(rhs_coll != nullptr, "Trying to vectorize two non-collections");

	Array array;
	for (auto i = 0u; i < rhs_coll->size(); ++i) {
		array.elements.push_back(
			Try(operation(interpreter, { lhs, Try(rhs_coll->index(interpreter, i)) })));
	}
	return array;
}

/// Intrinsic implementation primitive to ease operation vectorization
/// @invariant args.size() == 2
static Result<Value> vectorize(auto &&operation, Interpreter &interpreter, std::vector<Value> args)
{
	ensure(args.size() == 2, "Vectorization primitive only supports two arguments");
	return vectorize(std::move(operation), interpreter, std::move(args.front()), std::move(args.back()));
}

/// Helper simlifiing implementation of symetric binary operations.
///
/// Calls binary if values matches types any permutation of {t1, t2}, always in shape (t1, t2)
template<typename T1, typename T2>
inline auto symetric(Value &lhs, Value &rhs, auto binary)
	-> std::optional<decltype(
		std::apply(std::move(binary), *match<T1, T2>(lhs, rhs))
	)>
{
	if (auto a = match<T1, T2>(lhs, rhs)) {
		return std::apply(std::move(binary), *a);
	} else if (auto a = match<T1, T2>(rhs, lhs)) {
		return std::apply(std::move(binary), *a);
	} else {
		return std::nullopt;
	}
}

/// Creates implementation of plus/minus operator that support following operations:
///   number, number -> number (standard math operations)
///   n: number, m: music  -> music
///   m: music,  n: number -> music  moves m by n semitones (+ goes up, - goes down)
template<typename Binary_Operation>
static Result<Value> builtin_operator_add_subtract(Interpreter &interpreter, std::vector<Value> args)
{
	if (args.empty()) {
		return Number(0);
	}

	Value init = args.front();
	return algo::fold(std::span(args).subspan(1), std::move(init), [&interpreter](Value lhs, Value &rhs) -> Result<Value> {
		if (auto a = match<Number, Number>(lhs, rhs)) {
			return std::apply(Binary_Operation{}, *a);
		}

		auto result = symetric<Chord, Number>(lhs, rhs, [](Chord &lhs, Number rhs) {
			for (auto &note : lhs.notes) {
				if (note.base) {
					*note.base = Binary_Operation{}(*note.base, rhs.as_int());
					note.simplify_inplace();
				}
			}
			return lhs;
		});
		if (result.has_value()) {
			return *std::move(result);
		}

		if (holds_alternative<Collection>(lhs) != holds_alternative<Collection>(rhs)) {
			return vectorize(builtin_operator_add_subtract<Binary_Operation>, interpreter, std::move(lhs), std::move(rhs));
		}

		static_assert(std::is_same_v<std::plus<>, Binary_Operation> || std::is_same_v<std::minus<>, Binary_Operation>,
				"Error message printing only supports operators given above");

		return errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Operator,
			.name = std::is_same_v<std::plus<>, Binary_Operation> ? "+" : "-",
			.possibilities = {
				"(number, number) -> number",
				"(music, number) -> music",
				"(number, music) -> music",
				"(array, number|music) -> array",
				"(number|music, array) -> array",
			}
		};
	});
}

template<typename Binary_Operation, char ...Chars>
static Result<Value> builtin_operator_arithmetic(Interpreter& interpreter, std::vector<Value> args)
{
	static constexpr char Name[] = { Chars..., '\0' };
	if (args.empty()) {
		return Number(1);
	}
	auto init = std::move(args.front());
	return algo::fold(std::span(args).subspan(1), std::move(init),
		[&interpreter](Value lhs, Value &rhs) -> Result<Value> {
			if (auto a = match<Number, Number>(lhs, rhs)) {
				return std::apply(Binary_Operation{}, *a);
			}

			if (holds_alternative<Collection>(lhs) != holds_alternative<Collection>(rhs)) {
				return vectorize(builtin_operator_arithmetic<Binary_Operation, Chars...>, interpreter, std::move(lhs), std::move(rhs));
			}

			return errors::Unsupported_Types_For {
				.type = errors::Unsupported_Types_For::Operator,
				.name = Name,
				.possibilities = {
					"(number, number) -> number",
					"(array, number) -> array",
					"(number, array) -> array"
				}
			};
		}
	);
}

template<typename Binary_Predicate>
static Result<Value> builtin_operator_compare(Interpreter &interpreter, std::vector<Value> args)
{
	if (args.size() != 2) {
		return algo::pairwise_all(std::move(args), Binary_Predicate{});
	}

	auto lhs_coll = get_if<Collection>(args.front());
	auto rhs_coll = get_if<Collection>(args.back());

	if (bool(lhs_coll) != bool(rhs_coll)) {
		auto coll    = lhs_coll ? lhs_coll     : rhs_coll;
		auto element = lhs_coll ? &args.back() : &args.front();

		std::vector<Value> result;
		result.reserve(coll->size());
		for (auto i = 0u; i < coll->size(); ++i) {
			result.push_back(Value(
				Binary_Predicate{}(
					Try(coll->index(interpreter, i)),
					*element
				))
			);
		}
		return Array { std::move(result) };
	}

	return Binary_Predicate{}(std::move(args.front()), std::move(args.back()));
}

static Result<Value> builtin_operator_multiply(Interpreter &interpreter, std::vector<Value> args)
{
	if (args.empty()) {
		return Number(1);
	}

	auto init = std::move(args.front());
	return algo::fold(std::span(args).subspan(1), std::move(init), [&interpreter](Value lhs, Value &rhs) -> Result<Value> {
		{
			auto result = symetric<Number, Chord>(lhs, rhs, [](Number lhs, Chord &rhs) {
				return Array { std::vector<Value>(lhs.floor().as_int(), std::move(rhs)) };
			});

			if (result.has_value()) {
				return *std::move(result);
			}
		}
		{
			auto result = symetric<Number, Collection>(lhs, rhs, [&interpreter](Number lhs, Collection &rhs) -> Result<Value> {
				std::vector<Value> values;
				values.reserve(rhs.size() * lhs.floor().as_int());
				for (unsigned j = 0; j < lhs.floor().as_int(); ++j) {
					for (unsigned i = 0; i < rhs.size(); ++i) {
						values.push_back(Try(rhs.index(interpreter, i)));
					}
				}
				return values;
			});

			if (result.has_value()) {
				return *std::move(result);
			}
		}

		// If builtin_operator_arithmetic returns an error that lists all possible overloads
		// of this operator we must inject overloads that we provided above
		auto result = builtin_operator_arithmetic<std::multiplies<>, '*'>(interpreter, { std::move(lhs), std::move(rhs) });
		if (!result.has_value()) {
			auto &details = result.error().details;
			if (auto p = std::get_if<errors::Unsupported_Types_For>(&details)) {
				p->possibilities.push_back("(repeat: number, what: music) -> array of music");
				p->possibilities.push_back("(what: music, repeat: number) -> array of music");
			}
		}

		return result;
	});
}

static Result<Value> builtin_operator_index(Interpreter &interpreter, std::vector<Value> args)
{
	if (auto a = match<Collection, Number>(args)) {
		auto& [coll, pos] = *a;
		return coll.index(interpreter, pos.as_int());
	}

	if (auto a = match<Collection, Bool>(args)) {
		if (auto& [coll, pos] = *a; pos) {
			return coll.index(interpreter, 0);
		}
		return Value{};
	}

	if (auto a = match<Collection, Collection>(args)) {
		auto& [source, positions] = *a;

		std::vector<Value> result;
		for (size_t n = 0; n < positions.size(); ++n) {
			auto const v = Try(positions.index(interpreter, n));

			auto index = std::visit(Overloaded {
				[](Number n) -> std::optional<size_t> { return n.floor().as_int(); },
				[n](Bool b)  -> std::optional<size_t> { return b ? std::optional(n) : std::nullopt; },
				[](auto &&)  -> std::optional<size_t> { return std::nullopt; }
			}, v.data);

			if (index) {
				result.push_back(Try(source.index(interpreter, *index)));
			}
		}
		return Array(std::move(result));
	}

	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Operator,
			.name = ".",
			.possibilities = {
				"array . bool -> bool",
				"array . number -> any",
				"array . (array of numbers) -> array",
			},
		},
	};
}

static Result<Value> builtin_operator_join(Interpreter &interpreter, std::vector<Value> args)
{
	constexpr auto guard = Guard<2> {
		.name = "&",
		.possibilities = {
			"(array, array) -> array",
			"(music, music) -> music",
		},
		.type = errors::Unsupported_Types_For::Operator
	};

	if (auto a = match<Chord, Chord>(args)) {
		auto [lhs, rhs] = *a;
		auto &l = lhs.notes, &r = rhs.notes;
		if (l.size() < r.size()) {
			std::swap(l, r);
			std::swap(lhs, rhs);
		}

		// Append one set of notes to another to make bigger chord!
		l.reserve(l.size() + r.size());
		std::move(r.begin(), r.end(), std::back_inserter(l));

		return lhs;
	}

	auto result = Array {};
	for (auto&& a : args) {
		auto &array = *Try(guard.match<Collection>(a));
		for (auto n = 0u; n < array.size(); ++n) {
			result.elements.push_back(Try(array.index(interpreter, n)));
		}
	}
	return result;
}


using Operator_Entry = std::tuple<char const*, Intrinsic::Function_Pointer>;

using power = decltype([](Number lhs, Number rhs) -> Result<Number> {
	return lhs.pow(rhs);
});

/// Operators definition table
static constexpr auto Operators = std::array {
	Operator_Entry { "+",  builtin_operator_add_subtract<std::plus<>> },
	Operator_Entry { "-",  builtin_operator_add_subtract<std::minus<>> },
	Operator_Entry { "*",  builtin_operator_multiply },
	Operator_Entry { "/",  builtin_operator_arithmetic<std::divides<>, '/'> },
	Operator_Entry { "%",  builtin_operator_arithmetic<std::modulus<>, '%'> },
	Operator_Entry { "**", builtin_operator_arithmetic<power, '*', '*'> },

	Operator_Entry { "!=", builtin_operator_compare<std::not_equal_to<>> },
	Operator_Entry { "<",  builtin_operator_compare<std::less<>> },
	Operator_Entry { "<=", builtin_operator_compare<std::less_equal<>> },
	Operator_Entry { "==", builtin_operator_compare<std::equal_to<>> },
	Operator_Entry { ">",  builtin_operator_compare<std::greater<>> },
	Operator_Entry { ">=", builtin_operator_compare<std::greater_equal<>> },

	Operator_Entry { "[", builtin_operator_index },
	Operator_Entry { "&", builtin_operator_join },
};

// All operators should be defined here except '=', 'and' and 'or' which handle evaluation differently
// and are need unevaluated expressions for their proper evaluation. Exclusion of them is marked
// as subtraction of total excluded operators from expected constant
static_assert(Operators.size() == Operators_Count - 3, "All operators handlers are defined here");

void Interpreter::register_builtin_operators()
{
	// Set all predefined operators into operators array
	for (auto &[name, fptr] : Operators) { operators[name] = fptr; }
}

Result<Value> builtin_prefix_operator_plus(Interpreter &interpreter, std::vector<Value> args)
{
	(void)interpreter;
	(void)args;
	unimplemented();
}

Result<Value> builtin_prefix_operator_minus(Interpreter &interpreter, std::vector<Value> args)
{
	ensure(args.size() == 1, "Unary operation only allows for one argument");

	if (auto a = match<Number>(args)) {
		auto const& [number] = *a;
		return -number;
	}

	if (auto a = match<Collection>(args)) {
		auto const& [collection] = *a;
		std::vector<Value> result;

		for (auto i = 0u; i < collection.size(); ++i) {
			auto value = Try(collection.index(interpreter, i));
			if (auto a = match<Number>(value)) {
				auto const& [number] = *a;
				result.push_back(-number);
			}
		}

		return result;
	}

	unimplemented();
}
