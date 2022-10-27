#include <musique/algo.hh>
#include <musique/interpreter/env.hh>
#include <musique/guard.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>

#include <random>
#include <memory>
#include <iostream>
#include <unordered_set>
#include <chrono>
#include <thread>

/// Check if type has index method
template<typename T>
concept With_Index_Method = requires (T &t, Interpreter interpreter, usize position) {
	{ t.index(interpreter, position) } -> std::convertible_to<Result<Value>>;
};

/// Check if type has either (index operator or method) and size() method
template<typename T>
concept Iterable = (With_Index_Method<T> || With_Index_Operator<T>) && requires (T const& t) {
	{ t.size() } -> std::convertible_to<usize>;
};

static Result<std::vector<Value>> deep_flat(Interpreter &interpreter, Iterable auto const& array)
{
	std::vector<Value> result;

	for (auto i = 0u; i < array.size(); ++i) {
		Value element;
		if constexpr (With_Index_Method<decltype(array)>) {
			element = Try(array.index(interpreter, i));
		} else {
			element = std::move(array[i]);
		}

		if (auto collection = get_if<Collection>(element)) {
			auto array = Try(deep_flat(interpreter, *collection));
			std::move(array.begin(), array.end(), std::back_inserter(result));
		} else {
			result.push_back(std::move(element));
		}
	}

	return result;
}

/// Create chord out of given notes
template<Iterable T>
static inline std::optional<Error> create_chord(std::vector<Note> &chord, Interpreter &interpreter, T&& args)
{
	for (auto i = 0u; i < args.size(); ++i) {
		Value arg;
		if constexpr (With_Index_Method<T>) {
			arg = Try(args.index(interpreter, i));
		} else {
			arg = std::move(args[i]);
		}

		if (auto arg_chord = get_if<Chord>(arg)) {
			std::copy_if(
				arg_chord->notes.begin(), arg_chord->notes.end(),
				std::back_inserter(chord),
				[](Note const& n) { return n.base.has_value(); }
			);
			continue;
		}

		if (auto collection = get_if<Collection>(arg)) {
			Try(create_chord(chord, interpreter, *collection));
			continue;
		}

		ensure(false, "this type is not supported inside chord"); // TODO(assert)
	}

	return {};
}

/// Define handler for members of context that allow reading and writing to them
template<auto Mem_Ptr>
static Result<Value> ctx_read_write_property(Interpreter &interpreter, std::vector<Value> args)
{
	ensure(args.size() <= 1, "Ctx get or set is only supported (wrong number of arguments)"); // TODO(assert)

	using Member_Type = std::remove_cvref_t<decltype(std::declval<Context>().*(Mem_Ptr))>;

	if (args.size() == 0) {
		return Number(interpreter.context_stack.back().*(Mem_Ptr));
	}

	ensure(std::holds_alternative<Number>(args.front().data), "Ctx only holds numeric values");

	if constexpr (std::is_same_v<Member_Type, Number>) {
		interpreter.context_stack.back().*(Mem_Ptr) = std::get<Number>(args.front().data);
	} else {
		interpreter.context_stack.back().*(Mem_Ptr) = static_cast<Member_Type>(
			std::get<Number>(args.front().data).as_int()
		);
	}

	return Value{};
}

/// Iterate over array and it's subarrays to create one flat array
static Result<Array> into_flat_array(Interpreter &interpreter, std::span<Value> args)
{
	Array target;
	for (auto &arg : args) {
		std::visit(Overloaded {
			[&target](Array &&array) -> std::optional<Error> {
				std::move(array.elements.begin(), array.elements.end(), std::back_inserter(target.elements));
				return {};
			},
			[&target, &interpreter](Block &&block) -> std::optional<Error> {
				for (auto i = 0u; i < block.size(); ++i) {
					target.elements.push_back(Try(block.index(interpreter, i)));
				}
				return {};
			},
			[&target, &arg](auto&&) -> std::optional<Error> {
				target.elements.push_back(std::move(arg));
				return {};
			},
		}, std::move(arg.data));
	}
	return target;
}

static Result<Array> into_flat_array(Interpreter &i, std::vector<Value> args)
{
	return into_flat_array(i, std::span(args));
}

/// Helper to convert method to it's name
template<auto> struct Number_Method_Name;
template<> struct Number_Method_Name<&Number::floor> { static constexpr auto value = "floor"; };
template<> struct Number_Method_Name<&Number::ceil>  { static constexpr auto value = "ceil"; };
template<> struct Number_Method_Name<&Number::round> { static constexpr auto value = "round"; };

/// Apply method like Number::floor to arguments
template<auto Method>
static Result<Value> apply_numeric_transform(Interpreter &i, std::vector<Value> args)
{
	if (args.size()) {
		if (auto number = get_if<Number>(args.front().data)) {
			return (number->*Method)();
		}
	}

	auto array = Try(into_flat_array(i, std::span(args)));
	for (Value &arg : array.elements) {
		if (auto number = get_if<Number>(arg.data)) {
			*number = (number->*Method)();
		} else {
			goto invalid_argument_type;
		}
	}
	return array;


invalid_argument_type:
	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Function,
			.name = Number_Method_Name<Method>::value,
			.possibilities = {
				"(number | array of number ...) -> number",
			}
		},
	};
}

/// Direction used in range definition (up -> 1, 2, 3; down -> 3, 2, 1)
enum class Range_Direction { Up, Down };

/// Create range according to direction and specification, similar to python
template<Range_Direction dir>
static Result<Value> builtin_range(Interpreter&, std::vector<Value> args)
{
	auto start = Number(0), stop = Number(0), step = Number(1);

	if (0) {}
	else if (auto a = match<Number>(args))                 { std::tie(stop)              = *a; }
	else if (auto a = match<Number, Number>(args))         { std::tie(start, stop)       = *a; }
	else if (auto a = match<Number, Number, Number>(args)) { std::tie(start, stop, step) = *a; }
	else {
		return errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Function,
			.name = "range",
			.possibilities = {
				"(stop: number) -> array of number",
				"(start: number, stop: number) -> array of number",
				"(start: number, stop: number, step: number) -> array of number",
			}
		};
	}

	Array array;
	if constexpr (dir == Range_Direction::Up) {
		for (; start < stop; start += step) {
			array.elements.push_back(start);
		}
	} else {
		for (; stop > start; stop -= step) {
			array.elements.push_back(stop - Number(1));
		}
	}
	return array;
}

/// Send MIDI Program Change message
static auto builtin_program_change(Interpreter &i, std::vector<Value> args) -> Result<Value> {
	if (auto a = match<Number>(args)) {
		auto [program] = *a;
		i.midi_connection->send_program_change(0, program.as_int());
		return Value{};
	}

	if (auto a = match<Number, Number>(args)) {
		auto [chan, program] = *a;
		i.midi_connection->send_program_change(chan.as_int(), program.as_int());
		return Value{};
	}

	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Function,
			.name = "program_change",
			.possibilities = {
				"(number) -> nil",
				"(number, number) -> nil",
			}
		},
		.location = {}
	};
}

/// Plays sequentialy notes walking into arrays and evaluation blocks
///
/// @invariant default_action is play one
static inline std::optional<Error> sequential_play(Interpreter &i, Value v)
{
	if (auto array = get_if<Array>(v)) {
		for (auto &el : array->elements) {
			Try(sequential_play(i, std::move(el)));
		}
	}
	else if (auto block = get_if<Block>(v)) {
		Try(sequential_play(i, Try(i.eval(std::move(block->body)))));
	}
	else if (auto chord = get_if<Chord>(v)) {
		return i.play(*chord);
	}

	return {};
}

/// Play what's given
static std::optional<Error> action_play(Interpreter &i, Value v)
{
	Try(sequential_play(i, std::move(v)));
	return {};
}

/// Play notes
template<With_Index_Operator Container = std::vector<Value>>
static inline Result<Value> builtin_play(Interpreter &i, Container args)
{
	Try(ensure_midi_connection_available(i, "play"));
	auto previous_action = std::exchange(i.default_action, action_play);
	i.context_stack.push_back(i.context_stack.back());

	auto const finally = [&] {
		i.default_action = std::move(previous_action);
		i.context_stack.pop_back();
	};

	for (auto &el : args) {
		if (std::optional<Error> error = sequential_play(i, std::move(el))) {
			finally();
			return *std::move(error);
		}
	}

	finally();
	return {};
}

/// Play first argument while playing all others
static Result<Value> builtin_par(Interpreter &i, std::vector<Value> args) {
	Try(ensure_midi_connection_available(i, "par"));

	ensure(args.size() >= 1, "par only makes sense for at least one argument"); // TODO(assert)
	if (args.size() == 1) {
		auto chord = get_if<Chord>(args.front());
		ensure(chord, "Par expects music value as first argument"); // TODO(assert)
		Try(i.play(std::move(*chord)));
		return Value{};
	}

	// Create chord that should sustain during playing of all other notes
	auto &ctx = i.context_stack.back();
	auto chord = get_if<Chord>(args.front());
	ensure(chord, "par expects music value as first argument"); // TODO(assert)

	std::for_each(chord->notes.begin(), chord->notes.end(), [&](Note &note) { note = ctx.fill(note); });

	for (auto const& note : chord->notes) {
		if (note.base) {
			i.midi_connection->send_note_on(0, *note.into_midi_note(), 127);
		}
	}

	auto result = builtin_play(i, std::span(args).subspan(1));

	for (auto const& note : chord->notes) {
		if (note.base) {
			i.midi_connection->send_note_off(0, *note.into_midi_note(), 127);
		}
	}
	return result;
}

/// Plays each argument simultaneously
static Result<Value> builtin_sim(Interpreter &interpreter, std::vector<Value> args)
{
	// Simplest solution that will allow arbitrary code will be to clone Interpreter
	// and execute code recording all messages that are beeing sent. Then sort it
	// accordingly and start playing. Unfortuanatelly this solution will not respect
	// cause and effect chains - [play c; say 42; play d] will first say 42,
	// then play c and d
	//
	// The next solution is to run code in multithreaded context, making sure that
	// all operations will be locked accordingly. This would require support from
	// parser to well behave under multithreaded environment which now cannot be
	// guaranteeed.
	//
	// The final solution which is only partially working is to traverse arguments
	// in this function and build a schedule that will be executed

	// 1. Resolve all notes from arguments to tracks

	std::vector<std::vector<Chord>> tracks(args.size());

	struct {
		Interpreter &interpreter;

		std::optional<Error> operator()(std::vector<Chord> &track, Value &arg)
		{
			if (auto chord = get_if<Chord>(arg)) {
				track.push_back(*chord);
				return {};
			}

			if (auto collection = get_if<Collection>(arg)) {
				for (auto i = 0u; i < collection->size(); ++i) {
					auto value = Try(collection->index(interpreter, i));
					Try((*this)(track, value));
				}
				return {};
			}

			// Invalid type for sim function
			return Error{errors::Unsupported_Types_For {
				.type = errors::Unsupported_Types_For::Function,
				.name = "sim",
				.possibilities = {
					"(music | array of music)+"
				},
			}};
		}
	} append { interpreter };

	for (auto i = 0u; i < args.size(); ++i) {
		Try(append(tracks[i], args[i]));
	}

	// 2. Translate tracks of notes into one timeline with on and off messages
	struct Instruction
	{
		Number when;
		enum { On, Off } action;
		uint8_t note;
	};
	std::vector<Instruction> schedule;

	auto const& ctx = interpreter.context_stack.back();

	for (auto const& track : tracks) {
		auto passed_time = Number(0);

		for (auto const& chord : track) {
			auto chord_length = Number(0);
			for (auto &note : chord.notes) {
				auto n = ctx.fill(note);
				auto const length = n.length.value();

				if (note.base) {
					auto const midi_note = n.into_midi_note().value();
					schedule.push_back({ .when = passed_time,          .action = Instruction::On,  .note = midi_note });
					schedule.push_back({ .when = passed_time + length, .action = Instruction::Off, .note = midi_note });
				}

				chord_length = std::max(n.length.value(), chord_length);
			}
			passed_time += chord_length;
		}
	}

	// 3. Sort timeline so events will be played at right time
	std::sort(schedule.begin(), schedule.end(), [](Instruction const& lhs, Instruction const& rhs) {
		return lhs.when < rhs.when;
	});

	// 4. Play according to timeline
	auto start_time = std::chrono::duration<float>(0);
	for (auto const& instruction : schedule) {
		auto const dur = ctx.length_to_duration({instruction.when});
		if (start_time < dur) {
			std::this_thread::sleep_for(dur - start_time);
			start_time = dur;
		}
		switch (instruction.action) {
		break; case Instruction::On:  interpreter.midi_connection->send_note_on(0, instruction.note, 127);
		break; case Instruction::Off: interpreter.midi_connection->send_note_off(0, instruction.note, 127);
		}
	}

	return Value{};
}

/// Calculate upper bound for sieve that has to yield n primes
///
/// Based on https://math.stackexchange.com/a/3678200
static inline size_t upper_sieve_bound_to_yield_n_primes(size_t n_primes)
{
	if (n_primes < 4) { return 10; }

	float n = n_primes;
	float xprev = 0;
	float x = n * std::log(n);
	float const max_diff = 0.5;

	while (x - xprev > max_diff) {
		xprev = x;
		x = n*std::log(x);
	}

	return std::ceil(x);
}

/// Generate n primes
static Result<Value> builtin_primes(Interpreter&, std::vector<Value> args)
{
	if (auto a = match<Number>(args)) {
		auto [n_frac] = *a;
		// Better sieve could be Sieve of Atkin, but it's more complicated
		// so for now we would use Eratosthenes one.
		if (n_frac.simplify_inplace(); n_frac.num <= 1) {
			return Array{};
		}
		size_t n = n_frac.floor().as_int();

		// Would preallocation be faster? Needs to be tested.
		// Using approximation of prime-counting formula pi(n) ≈ x / ln(x)
		size_t const approx_prime_count = n / std::log(float(n));
		std::vector<Value> results;
		results.reserve(approx_prime_count);

		// False means we have prime here
		auto const sieve_size = upper_sieve_bound_to_yield_n_primes(n);
		std::vector<bool> sieve(sieve_size, false);

		for (uint i = 2; i*i < sieve.size(); ++i) {
			if (sieve[i] == false) {
				for (uint j = i * i; j < sieve.size(); j += i) {
					sieve[j] = true;
				}
			}
		}

		for (uint i = 2; i < sieve.size() && results.size() != n; ++i) {
			if (!sieve[i]) {
				results.push_back(Number(i));
			}
		}
		return results;
	}

	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Function,
			.name = "nprimes",
			.possibilities = {
				"(number) -> array of number"
			}
		},
	};
}

/// Iterate over container
static Result<Value> builtin_for(Interpreter &i, std::vector<Value> args)
{
	constexpr auto guard = Guard<1> {
		.name = "for",
		.possibilities = { "(array, callback) -> any" }
	};

	if (auto a = match<Collection, Function>(args)) {
		auto& [collection, func] = *a;
		Value result{};
		for (size_t n = 0; n < collection.size(); ++n) {
			result = Try(func(i, { Try(collection.index(i, n)) }));
		}
		return result;
	} else {
		return guard.yield_error();
	}
}

/// Fold container
static Result<Value> builtin_fold(Interpreter &interpreter, std::vector<Value> args) {
	constexpr auto guard = Guard<2> {
		.name = "fold",
		.possibilities = {
			"(callback, ...values) -> any",
		}
	};

	if (args.size()) {
		if (auto p = get_if<Function>(args.front())) {
			auto xs = Try(flatten(interpreter, std::span(args).subspan(1)));
			if (xs.empty()) {
				return Value{};
			}
			auto init = xs[0];
			for (auto i = 1u; i < xs.size(); ++i) {
				init = Try((*p)(interpreter, { std::move(init), std::move(xs[i]) }));
			}
			return init;
		}
	}

	return guard.yield_error();
}

static Result<Value> builtin_map(Interpreter &interpreter, std::vector<Value> args)
{
	static constexpr auto guard = Guard<2> {
		.name = "map",
		.possibilities = {
			"(callback, array) -> any"
		}
	};

	if (args.empty()) {
		return guard.yield_error();
	}

	auto function = Try(guard.match<Function>(args.front()));

	std::vector<Value> result;

	for (auto &arg : std::span(args).subspan(1)) {
		if (auto collection = get_if<Collection>(arg)) {
			for (auto i = 0u; i < collection->size(); ++i) {
				auto element = Try(collection->index(interpreter, i));
				result.push_back(Try((*function)(interpreter, { std::move(element) })));
			}
		} else {
			result.push_back(Try((*function)(interpreter, { std::move(arg) })));
		}
	}

	return result;
}

/// Scan computes inclusive prefix sum
static Result<Value> builtin_scan(Interpreter &interpreter, std::vector<Value> args)
{
	if (args.size()) {
		if (auto p = get_if<Function>(args.front())) {
			auto xs = Try(flatten(interpreter, std::span(args).subspan(1)));
			for (auto i = 1u; i < xs.size(); ++i) {
				xs[i] = Try((*p)(interpreter, { xs[i-1], xs[i] }));
			}
			return xs;
		}
	}

	return errors::Unsupported_Types_For {
		.type = errors::Unsupported_Types_For::Function,
		.name = "scan",
		.possibilities = {
			"(callback, ...array) -> any"
		}
	};
}

/// Execute blocks depending on condition
static Result<Value> builtin_if(Interpreter &i, std::span<Ast> args)  {
	static constexpr auto guard = Guard<2> {
		.name = "if",
		.possibilities = {
			"(any, function) -> any",
			"(any, function, function) -> any"
		}
	};

	if (args.size() != 2 && args.size() != 3) {
		return guard.yield_error();
	}

	if (Try(i.eval((Ast)args.front())).truthy()) {
		if (args[1].type == Ast::Type::Block) {
			return Try(i.eval((Ast)args[1].arguments.front()));
		} else {
			return Try(i.eval((Ast)args[1]));
		}
	} else if (args.size() == 3) {
		if (args[2].type == Ast::Type::Block) {
			return Try(i.eval((Ast)args[2].arguments.front()));
		} else {
			return Try(i.eval((Ast)args[2]));
		}
	}

	return Value{};
}

/// Loop block depending on condition
static Result<Value> builtin_while(Interpreter &i, std::span<Ast> args)  {
	static constexpr auto guard = Guard<2> {
		.name = "while",
		.possibilities = {
			"(any, function) -> any"
		}
	};

	if (args.size() != 2) {
		return guard.yield_error();
	}

	while (Try(i.eval((Ast)args.front())).truthy()) {
		if (args[1].type == Ast::Type::Block) {
			Try(i.eval((Ast)args[1].arguments.front()));
		} else {
			Try(i.eval((Ast)args[1]));
		}
	}
	return Value{};
}

/// Try executing all but last block and if it fails execute last one
static Result<Value> builtin_try(Interpreter &interpreter, std::vector<Value> args)
{
	constexpr auto guard = Guard<1> {
		.name = "try",
		.possibilities = {
			"(...function) -> any"
		}
	};

	if (args.size() == 1) {
		auto callable = Try(guard.match<Function>(args[0]));
		return std::move(*callable)(interpreter, {}).value_or(Value{});
	}

	Value success;

	for (usize i = 0; i+1 < args.size(); ++i) {
		auto callable = Try(guard.match<Function>(args[i]));
		if (auto result = std::move(*callable)(interpreter, {})) {
			success = *std::move(result);
		} else {
			auto callable = Try(guard.match<Function>(args.back()));
			return std::move(*callable)(interpreter, {});
		}
	}

	return success;
}

/// Update value inside of array
static Result<Value> builtin_update(Interpreter &i, std::vector<Value> args)
{
	auto const guard = Guard<1> {
		.name = "update",
		.possibilities = {
			"array index:number new_value"
		}
	};

	if (args.size() != 3) {
		return guard.yield_error();
	}

	if (auto a = match<Array, Number, Value>(args)) {
		auto& [v, index, value] = *a;
		v.elements[index.as_int()] = std::move(std::move(value));
		return std::move(v);
	}

	if (auto a = match<Block, Number, Value>(args)) {
		auto& [v, index, value] = *a;
		auto array = Try(flatten(i, { std::move(v) }));
		array[index.as_int()] = std::move(args.back());
		return array;
	}

	return guard.yield_error();
}

/// Return typeof variable
static Result<Value> builtin_typeof(Interpreter&, std::vector<Value> args)
{
	ensure(args.size() == 1, "typeof expects only one argument"); // TODO(assert)
	return Symbol(type_name(args.front()));
}

/// Return length of container or set/get default length to play
static Result<Value> builtin_len(Interpreter &i, std::vector<Value> args)
{
	if (args.size() == 1) {
		if (auto coll = get_if<Collection>(args.front())) {
			return Number(coll->size());
		}
	}
	// TODO Add overload that tells length of array to error reporting
	return ctx_read_write_property<&Context::length>(i, std::move(args));
}

Result<Value> traverse(Interpreter &interpreter, Value &&value, auto &&lambda)
{
	if (auto collection = get_if<Collection>(value)) {
		std::vector<Value> flat;
		for (auto i = 0u; i < collection->size(); ++i) {
			Value v = Try(collection->index(interpreter, i));
			flat.push_back(Try(traverse(interpreter, std::move(v), lambda)));
		}
		return flat;
	}

	std::visit([&lambda]<typename T>(T &value) {
		if constexpr (requires { {lambda(value)}; }) {
			lambda(value);
		}
	}, value.data);
	return value;
}

/// Set length (first argument) to all other arguments preserving their shape
static Result<Value> builtin_set_len(Interpreter &interpreter, std::vector<Value> args)
{
	if (auto len = get_if<Number>(args.front())) {
		std::vector<Value> result;
		for (auto &arg : std::span(args).subspan(1)) {
			auto arg_result = Try(traverse(interpreter, std::move(arg), [&](Chord &c) {
				for (Note &note : c.notes) {
					note.length = *len;
				}
			}));
			result.push_back(std::move(arg_result));
		}
		return result;
	}

	return errors::Unsupported_Types_For {
		.type = errors::Unsupported_Types_For::Function,
		.name = "set_len",
		.possibilities = {
			"TODO"
		}
	};
}

static Result<Value> builtin_duration(Interpreter &interpreter, std::vector<Value> args)
{
	auto total = Number{};
	for (auto &arg : args) {
		Try(traverse(interpreter, std::move(arg), [&](Chord &c) {
			for (Note &note : c.notes) {
				if (note.length) {
					total += *note.length;
				}
			}
		}));
	}
	return total;
}

/// Join arguments into flat array
static Result<Value> builtin_flat(Interpreter &i, std::vector<Value> args)
{
	return Try(into_flat_array(i, std::move(args)));
}

/// Shuffle arguments
static Result<Value> builtin_shuffle(Interpreter &i, std::vector<Value> args)
{
	static std::mt19937 rnd{std::random_device{}()};
	auto array = Try(flatten(i, std::move(args)));
	std::shuffle(array.begin(), array.end(), rnd);
	return array;
}

/// Permute arguments
static Result<Value> builtin_permute(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	std::next_permutation(array.begin(), array.end());
	return array;
}

/// Sort arguments
static Result<Value> builtin_sort(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	std::sort(array.begin(), array.end());
	return array;
}

/// Reverse arguments
static Result<Value> builtin_reverse(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	std::reverse(array.begin(), array.end());
	return array;
}

/// Get minimum of arguments
static Result<Value> builtin_min(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(deep_flat(i, args));
	if (auto min = std::min_element(array.begin(), array.end()); min != array.end())
		return *min;
	return Value{};
}

/// Get maximum of arguments
static Result<Value> builtin_max(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(deep_flat(i, args));
	if (auto max = std::max_element(array.begin(), array.end()); max != array.end())
		return *max;
	return Value{};
}

/// Parition arguments into 2 arrays based on predicate
static Result<Value> builtin_partition(Interpreter &i, std::vector<Value> args)
{
	constexpr auto guard = Guard<1> {
		.name = "partition",
		.possibilities = { "(function, ...array) -> array" }
	};

	if (args.empty()) {
		return guard.yield_error();
	}

	auto& predicate = *Try(guard.match<Function>(args.front()));
	auto array = Try(flatten(i, std::span(args).subspan(1)));

	Array tuple[2] = {};
	for (auto &value : array) {
		tuple[Try(predicate(i, { value })).truthy()].elements.push_back(std::move(value));
	}

	return Array {{
		std::move(tuple[true]),
		std::move(tuple[false])
	}};
}

/// Rotate arguments by n steps
static Result<Value> builtin_rotate(Interpreter &i, std::vector<Value> args)
{
	constexpr auto guard = Guard<1> {
		.name = "rotate",
		.possibilities = { "(number, ...array) -> array" }
	};


	if (args.size()) {
		if (auto const offset_source = get_if<Number>(args.front())) {
			auto offset = offset_source->as_int();
			auto array = Try(flatten(i, std::span(args).subspan(1)));
			if (offset > 0) {
				offset = offset % array.size();
				std::rotate(array.begin(), array.begin() + offset, array.end());
			} else if (offset < 0) {
				offset = -offset % array.size();
				std::rotate(array.rbegin(), array.rbegin() + offset, array.rend());
			}
			return array;
		}
	}

	return guard.yield_error();
}

/// Returns unique collection of arguments
static Result<Value> builtin_unique(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, args));
	std::unordered_set<Value> seen;

	std::vector<Value> result;
	for (auto &el : array) {
		if (!seen.contains(el)) {
			seen.insert(el);
			result.push_back(std::move(el));
		}
	}
	return result;
}

/// Returns arguments with all successive copies eliminated
static Result<Value> builtin_uniq(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, args));

	std::optional<Value> previous;
	std::vector<Value> result;

	for (auto &el : array) {
		if (previous && *previous == el)
			continue;
		result.push_back(el);
		previous = std::move(el);
	}
	return result;
}

static Result<Value> builtin_hash(Interpreter&, std::vector<Value> args)
{
	return Number(
		std::accumulate(args.cbegin(), args.cend(), size_t(0), [](size_t h, Value const& v) {
			return hash_combine(h, std::hash<Value>{}(v));
		})
	);
}

/// Build chord from arguments
static Result<Value> builtin_chord(Interpreter &i, std::vector<Value> args)
{
	Chord chord;
	Try(create_chord(chord.notes, i, std::move(args)));
	return chord;
}

/// Send MIDI message Note On
static Result<Value> builtin_note_on(Interpreter &i, std::vector<Value> args)
{
	if (auto a = match<Number, Number, Number>(args)) {
		auto [chan, note, vel] = *a;
		i.midi_connection->send_note_on(chan.as_int(), note.as_int(), vel.as_int());
		return Value {};
	}

	if (auto a = match<Number, Chord, Number>(args)) {
		auto [chan, chord, vel] = *a;
		for (auto note : chord.notes) {
			note = i.context_stack.back().fill(note);
			i.midi_connection->send_note_on(chan.as_int(), *note.into_midi_note(), vel.as_int());
		}
	}

	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Function,
			.name = "note_on",
			.possibilities = {
				"(number, music, number) -> nil"
				"(number, number, number) -> nil",
			}
		},
		.location = {}
	};
}

/// Send MIDI message Note Off
static Result<Value> builtin_note_off(Interpreter &i, std::vector<Value> args)
{
	if (auto a = match<Number, Number>(args)) {
		auto [chan, note] = *a;
		i.midi_connection->send_note_off(chan.as_int(), note.as_int(), 127);
		return Value {};
	}

	if (auto a = match<Number, Chord>(args)) {
		auto& [chan, chord] = *a;

		for (auto note : chord.notes) {
			note = i.context_stack.back().fill(note);
			i.midi_connection->send_note_off(chan.as_int(), *note.into_midi_note(), 127);
		}
	}

	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Function,
			.name = "note_off",
			.possibilities = {
				"(number, music) -> nil"
				"(number, number) -> nil",
			}
		},
		.location = {}
	};
}

/// Interleaves arguments
static Result<Value> builtin_mix(Interpreter &i, std::vector<Value> args)
{
	std::vector<Value> result;

	std::unordered_map<std::size_t, std::size_t> indicies;

	size_t awaiting_containers = std::count_if(args.begin(), args.end(), holds_alternative<Collection>);

	// Algorithm description:
	// Repeat until all arguments were exhausted:
	//   For each argument:
	//     If it can be iterated then add current element to the list and move to the next one.
	//       If next element cannot be retrived mark container as exhausted and start from beginning
	//     Otherwise append element to the list
	do {
		for (size_t idx = 0; idx < args.size(); ++idx) {
			if (auto coll = get_if<Collection>(args[idx])) {
				result.push_back(Try(coll->index(i, indicies[idx]++ % coll->size())));
				if (indicies[idx] == coll->size()) {
					awaiting_containers--;
				}
			} else {
				result.push_back(args[idx]);
			}
		}
	} while (awaiting_containers);

	return result;
}

/// Call operator. Calls first argument with remaining arguments
static Result<Value> builtin_call(Interpreter &i, std::vector<Value> args)
{
	auto const guard = Guard<1> {
		.name = "call",
		.possibilities = {
			"(function, ...args) -> any"
		}
	};

	if (args.empty()) {
		return guard.yield_error();
	}

	auto fst = std::move(args.front());
	auto &callable = *Try(guard.match<Function>(fst));
	args.erase(args.begin());
	return callable(i, std::move(args));
}

void Interpreter::register_builtin_functions()
{
	auto &global = *Env::global;

	global.force_define("bpm",            ctx_read_write_property<&Context::bpm>);
	global.force_define("call",           builtin_call);
	global.force_define("ceil",           apply_numeric_transform<&Number::ceil>);
	global.force_define("chord",          builtin_chord);
	global.force_define("down",           builtin_range<Range_Direction::Down>);
	global.force_define("duration",       builtin_duration);
	global.force_define("flat",           builtin_flat);
	global.force_define("floor",          apply_numeric_transform<&Number::floor>);
	global.force_define("fold",           builtin_fold);
	global.force_define("for",            builtin_for);
	global.force_define("hash",           builtin_hash);
	global.force_define("if",             builtin_if);
	global.force_define("instrument",     builtin_program_change);
	global.force_define("len",            builtin_len);
	global.force_define("map",            builtin_map);
	global.force_define("max",            builtin_max);
	global.force_define("min",            builtin_min);
	global.force_define("mix",            builtin_mix);
	global.force_define("note_off",       builtin_note_off);
	global.force_define("note_on",        builtin_note_on);
	global.force_define("nprimes",        builtin_primes);
	global.force_define("oct",            ctx_read_write_property<&Context::octave>);
	global.force_define("par",            builtin_par);
	global.force_define("partition",      builtin_partition);
	global.force_define("permute",        builtin_permute);
	global.force_define("pgmchange",      builtin_program_change);
	global.force_define("play",           builtin_play);
	global.force_define("program_change", builtin_program_change);
	global.force_define("range",          builtin_range<Range_Direction::Up>);
	global.force_define("reverse",        builtin_reverse);
	global.force_define("rotate",         builtin_rotate);
	global.force_define("round",          apply_numeric_transform<&Number::round>);
	global.force_define("scan",           builtin_scan);
	global.force_define("set_len",        builtin_set_len);
	global.force_define("shuffle",        builtin_shuffle);
	global.force_define("sim",            builtin_sim);
	global.force_define("sort",           builtin_sort);
	global.force_define("try",            builtin_try);
	global.force_define("typeof",         builtin_typeof);
	global.force_define("uniq",           builtin_uniq);
	global.force_define("unique",         builtin_unique);
	global.force_define("up",             builtin_range<Range_Direction::Up>);
	global.force_define("update",         builtin_update);
	global.force_define("while",          builtin_while);
}
