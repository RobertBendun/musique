#include <musique.hh>
#include <musique_internal.hh>

/// Intrinsic implementation primitive to ease operation vectorization
Result<Value> vectorize(auto &&operation, Interpreter &interpreter, Value lhs, Value rhs)
{
	Array array;

	if (is_indexable(lhs.type) && !is_indexable(rhs.type)) {
		Array array;
		for (auto i = 0u; i < lhs.size(); ++i) {
			array.elements.push_back(
				Try(operation(interpreter, { Try(lhs.index(interpreter, i)), rhs })));
		}
		return Value::from(std::move(array));
	}

	for (auto i = 0u; i < rhs.size(); ++i) {
		array.elements.push_back(
			Try(operation(interpreter, { lhs, Try(rhs.index(interpreter, i)) })));
	}
	return Value::from(std::move(array));
}

/// Intrinsic implementation primitive to ease operation vectorization
/// @invariant args.size() == 2
Result<Value> vectorize(auto &&operation, Interpreter &interpreter, std::vector<Value> args)
{
	assert(args.size() == 2, "Vectorization primitive only supports two arguments");
	return vectorize(std::move(operation), interpreter, std::move(args.front()), std::move(args.back()));
}


std::optional<Value> symetric(Value::Type t1, Value::Type t2, Value &lhs, Value &rhs, auto binary_predicate)
{
	if (lhs.type == t1 && rhs.type == t2) {
		return binary_predicate(std::move(lhs), std::move(rhs));
	} else if (lhs.type == t2 && rhs.type == t1) {
		return binary_predicate(std::move(rhs), std::move(lhs));
	} else {
		return std::nullopt;
	}
}

/// Creates implementation of plus/minus operator that support following operations:
///   number, number -> number (standard math operations)
///   n: number, m: music  -> music
///   m: music,  n: number -> music  moves m by n semitones (+ goes up, - goes down)
template<typename Binary_Operation>
static Result<Value> plus_minus_operator(Interpreter &interpreter, std::vector<Value> args)
{
	static_assert(std::is_same_v<Binary_Operation, std::plus<>> || std::is_same_v<Binary_Operation, std::minus<>>,
		"Error reporting depends on only one of this two types beeing provided");

	using NN = Shape<Value::Type::Number, Value::Type::Number>;
	using MN = Shape<Value::Type::Music,  Value::Type::Number>;
	using NM = Shape<Value::Type::Number, Value::Type::Music>;

	if (NN::typecheck(args)) {
		auto [a, b] = NN::move_from(args);
		return Value::from(Binary_Operation{}(std::move(a), std::move(b)));
	}

	if (MN::typecheck(args)) {
		auto [chord, offset] = MN::move_from(args);
		for (auto &note : chord.notes) {
			if (note.base) {
				*note.base = Binary_Operation{}(*note.base, offset.as_int());
				note.simplify_inplace();
			}
		}
		return Value::from(std::move(chord));
	}

	if (NM::typecheck(args)) {
		auto [offset, chord] = NM::move_from(args);
		for (auto &note : chord.notes) {
			if (note.base) {
				*note.base = Binary_Operation{}(*note.base, offset.as_int());
				note.simplify_inplace();
			}
		}
		return Value::from(std::move(chord));
	}

	if (may_be_vectorized(args)) {
		return vectorize(plus_minus_operator<Binary_Operation>, interpreter, std::move(args));
	}

	// TODO Limit possibilities based on provided types
	static_assert(std::is_same_v<std::plus<>, Binary_Operation> || std::is_same_v<std::minus<>, Binary_Operation>,
			"Error message printing only supports operators given above");
	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Operator,
			.name = std::is_same_v<std::plus<>, Binary_Operation> ? "+" : "-",
			.possibilities = {
				"(number, number) -> number",
				"(music, number) -> music",
				"(number, music) -> music",
				"(array, number|music) -> array",
				"(number|music, array) -> array",
			}
		},
		.location = {}, // TODO fill location
	};
}

template<typename Binary_Operation, char ...Chars>
static Result<Value> binary_operator(Interpreter& interpreter, std::vector<Value> args)
{
	static constexpr char Name[] = { Chars..., '\0' };
	if (args.empty()) {
		return Value::from(Number(1));
	}
	auto init = std::move(args.front());
	return algo::fold(std::span(args).subspan(1), std::move(init),
		[&interpreter](Value lhs, Value &rhs) -> Result<Value> {
			if (lhs.type == Value::Type::Number && rhs.type == Value::Type::Number) {
				return wrap_value(Binary_Operation{}(std::move(lhs).n, std::move(rhs).n));
			}

			if (is_indexable(lhs.type) != is_indexable(rhs.type)) {
				return vectorize(binary_operator<Binary_Operation, Chars...>, interpreter, std::move(lhs), std::move(rhs));
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
static Result<Value> comparison_operator(Interpreter &interpreter, std::vector<Value> args)
{
	if (args.size() != 2) {
		return Value::from(algo::pairwise_all(std::move(args), Binary_Predicate{}));
	}

	if (is_indexable(args[1].type) && !is_indexable(args[0].type)) {
		std::swap(args[1], args[0]);
		goto vectorized;
	}

	if (is_indexable(args[0].type) && !is_indexable(args[1].type)) {
	vectorized:
		std::vector<Value> result;
		result.reserve(args[0].size());
		for (auto i = 0u; i < args[0].size(); ++i) {
			result.push_back(
				Value::from(
					Binary_Predicate{}(
						Try(args[0].index(interpreter, i)),
						args[1]
					)
				)
			);
		}
		return Value::from(Array { std::move(result) });
	}

	return Value::from(Binary_Predicate{}(std::move(args.front()), std::move(args.back())));
}

static Result<Value> multiplication_operator(Interpreter &i, std::vector<Value> args)
{
	if (args.empty()) {
		return Value::from(Number(1));
	}

	auto init = std::move(args.front());
	return algo::fold(std::span(args).subspan(1), std::move(init), [&i](Value lhs, Value &rhs) -> Result<Value> {
		{
			auto result = symetric(Value::Type::Number, Value::Type::Music, lhs, rhs, [](Value lhs, Value rhs) {
				return Value::from(Array {
					.elements = std::vector<Value>(lhs.n.floor().as_int(), std::move(rhs))
				});
			});

			if (result.has_value()) {
				return *std::move(result);
			}
		}

		// If binary_operator returns an error that lists all possible overloads
		// of this operator we must inject overloads that we provided above
		auto result = binary_operator<std::multiplies<>, '*'>(i, { std::move(lhs), std::move(rhs) });
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

using Operator_Entry = std::tuple<char const*, Intrinsic>;

using power = decltype([](Number lhs, Number rhs) -> Result<Number> {
	return lhs.pow(rhs);
});

/// Operators definition table
static constexpr auto Operators = std::array {
	Operator_Entry { "+", plus_minus_operator<std::plus<>> },
	Operator_Entry { "-", plus_minus_operator<std::minus<>> },
	Operator_Entry { "*", multiplication_operator },
	Operator_Entry { "/", binary_operator<std::divides<>, '/'> },
	Operator_Entry { "%", binary_operator<std::modulus<>, '%'> },
	Operator_Entry { "**", binary_operator<power, '*', '*'> },

	Operator_Entry { "!=", comparison_operator<std::not_equal_to<>> },
	Operator_Entry { "<",  comparison_operator<std::less<>> },
	Operator_Entry { "<=", comparison_operator<std::less_equal<>> },
	Operator_Entry { "==", comparison_operator<std::equal_to<>> },
	Operator_Entry { ">",  comparison_operator<std::greater<>> },
	Operator_Entry { ">=", comparison_operator<std::greater_equal<>> },

	Operator_Entry { ".",
		+[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			if (args.size() == 2 && is_indexable(args[0].type)) {
				if (args[1].type == Value::Type::Number) {
					return std::move(args.front()).index(i, std::move(args.back()).n.as_int());
				}

				if (args[1].type == Value::Type::Bool) {
					return std::move(args.front()).index(i, args.back().b ? 1 : 0);
				}

				if (is_indexable(args[1].type)) {
					std::vector<Value> result;
					for (size_t n = 0; n < args[1].size(); ++n) {
						auto const v = Try(args[1].index(i, n));
						switch (v.type) {
						break; case Value::Type::Number:
							result.push_back(Try(args[0].index(i, v.n.as_int())));

						break; case Value::Type::Bool: default:
							if (v.truthy()) {
								result.push_back(Try(args[0].index(i, n)));
							}
						}
					}
					return Value::from(Array { result });
				}
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
	},

	Operator_Entry { "&",
		+[](Interpreter& i, std::vector<Value> args) -> Result<Value> {
			constexpr auto guard = Guard<2> {
				.name = "&",
				.possibilities = {
					"(array, array) -> array",
					"(music, music) -> music",
				},
				.type = errors::Unsupported_Types_For::Operator
			};

			using Chord_Chord = Shape<Value::Type::Music, Value::Type::Music>;
			if (Chord_Chord::typecheck(args)) {
				auto [lhs, rhs] = Chord_Chord::move_from(args);
				auto &l = lhs.notes;
				auto &r = rhs.notes;

				// Append one set of notes to another to make bigger chord!
				l.reserve(l.size() + r.size());
				std::move(r.begin(), r.end(), std::back_inserter(l));

				return Value::from(lhs);
			}

			auto result = Array {};
			for (auto&& array : args) {
				Try(guard(is_indexable, array));
				for (auto n = 0u; n < array.size(); ++n) {
					result.elements.push_back(Try(array.index(i, n)));
				}
			}
			return Value::from(std::move(result));
		}
	},
};

// All operators should be defined here except 'and' and 'or' which handle evaluation differently
// and are need unevaluated expressions for their proper evaluation. Exclusion of them is marked
// as subtraction of total excluded operators from expected constant
static_assert(Operators.size() == Operators_Count - 2, "All operators handlers are defined here");

void Interpreter::register_builtin_operators()
{
	// Set all predefined operators into operators array
	for (auto &[name, fptr] : Operators) { operators[name] = fptr; }
}
