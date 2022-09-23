#ifndef Musique_Internal_HH
#define Musique_Internal_HH

#include <musique.hh>
#include <optional>
#include <numeric>
#include <ranges>

/// Allows creation of guards that ensure proper type
template<usize N>
struct Guard
{
	std::string_view name;
	std::array<std::string_view, N> possibilities;
	errors::Unsupported_Types_For::Type type = errors::Unsupported_Types_For::Function;

	inline Error yield_error() const
	{
		auto error = errors::Unsupported_Types_For {
			.type = type,
			.name = std::string(name),
			.possibilities = {}
		};
		std::transform(possibilities.begin(), possibilities.end(), std::back_inserter(error.possibilities), [](auto s) {
			return std::string(s);
		});
		return Error { std::move(error) };
	}

	inline std::optional<Error> yield_result() const
	{
		return yield_error();
	}

	inline std::optional<Error> operator()(bool(*predicate)(Value::Type), Value const& v) const
	{
		return predicate(v.type) ? std::optional<Error>{} : yield_result();
	}
};

/// Binary operation may be vectorized when there are two argument which one is indexable and other is not
static inline bool may_be_vectorized(std::vector<Value> const& args)
{
	return args.size() == 2 && (is_indexable(args[0].type) != is_indexable(args[1].type));
}

struct Interpreter::Incoming_Midi_Callbacks
{
	Value note_on{};
	Value note_off{};

	inline Incoming_Midi_Callbacks() = default;

	Incoming_Midi_Callbacks(Incoming_Midi_Callbacks &&) = delete;
	Incoming_Midi_Callbacks(Incoming_Midi_Callbacks const&) = delete;

	Incoming_Midi_Callbacks& operator=(Incoming_Midi_Callbacks &&) = delete;
	Incoming_Midi_Callbacks& operator=(Incoming_Midi_Callbacks const&) = delete;


	inline void add_callbacks(midi::Connection &midi, Interpreter &interpreter)
	{
		register_callback(midi.note_on_callback, note_on, interpreter);
		register_callback(midi.note_off_callback, note_off, interpreter);
	}

	template<typename ...T>
	inline void register_callback(std::function<void(T...)> &target, Value &callback, Interpreter &i)
	{
		if (&callback == &note_on || &callback == &note_off) {
			// This messages have MIDI note number as second value, so they should be represented
			// in our own note abstraction, not as numbers.
			target = [interpreter = &i, callback = &callback](T ...source_args)
			{
				if (callback->type != Value::Type::Nil) {
					std::vector<Value> args { Value::from(Number(source_args))... };
					args[1] = Value::from(Chord { .notes { Note {
						.base = i32(args[1].n.num % 12),
						.octave = args[1].n.num / 12
					}}});
					auto result = (*callback)(*interpreter, std::move(args));
					// We discard this since callback is running in another thread.
					(void) result;
				}
			};
		} else {
			// Generic case, preserve all passed parameters as numbers
			target = [interpreter = &i, callback = &callback](T ...source_args)
			{
				if (callback->type != Value::Type::Nil) {
					auto result = (*callback)(*interpreter, { Value::from(Number(source_args))...  });
					// We discard this since callback is running in another thread.
					(void) result;
				}
			};
		}
	}
};

enum class Midi_Connection_Type { Output, Input };
std::optional<Error> ensure_midi_connection_available(Interpreter&, Midi_Connection_Type, std::string_view operation_name);

constexpr std::size_t hash_combine(std::size_t lhs, std::size_t rhs) {
	return lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
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

/// Generic algorithms support
namespace algo
{
	/// Check if predicate is true for all successive pairs of elements
	constexpr bool pairwise_all(
		std::ranges::forward_range auto &&range,
		auto &&binary_predicate)
	{
		auto it = std::begin(range);
		auto const end_it = std::end(range);
		for (auto next_it = std::next(it); it != end_it && next_it != end_it; ++it, ++next_it) {
			if (not binary_predicate(*it, *next_it)) {
				return false;
			}
		}
		return true;
	}

	/// Fold that stops iteration on error value via Result type
	template<typename T>
	constexpr Result<T> fold(auto&& range, T init, auto &&reducer)
		requires (is_template_v<Result, decltype(reducer(std::move(init), *range.begin()))>)
	{
		for (auto &&value : range) {
			init = Try(reducer(std::move(init), value));
		}
		return init;
	}
}

#endif
