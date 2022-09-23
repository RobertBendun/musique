#include <musique.hh>
#include <musique_internal.hh>

#include <random>
#include <memory>
#include <iostream>
#include <unordered_set>
#include <chrono>
#include <thread>

void Interpreter::register_callbacks()
{
	assert(callbacks == nullptr, "This field should be uninitialized");
	callbacks = std::make_unique<Interpreter::Incoming_Midi_Callbacks>();
	callbacks->add_callbacks(*midi_connection, *this);
}

/// Check if type has index method
template<typename T>
concept With_Index_Method = requires (T t, Interpreter interpreter, usize position) {
	{ t.index(interpreter, position) } -> std::convertible_to<Result<Value>>;
};

/// Check if type has index operator
template<typename T>
concept With_Index_Operator = requires (T t, unsigned i) {
	{ t[i] } -> std::convertible_to<Value>;
};

/// Check if type has either (index operator or method) and size() method
template<typename T>
concept Iterable = (With_Index_Method<T> || With_Index_Operator<T>) && requires (T const t) {
	{ t.size() } -> std::convertible_to<usize>;
};

/// Create chord out of given notes
template<Iterable T>
static inline std::optional<Error> create_chord(std::vector<Note> &chord, Interpreter &interpreter, T args)
{
	for (auto i = 0u; i < args.size(); ++i) {
		Value arg;
		if constexpr (With_Index_Method<T>) {
			arg = Try(args.index(interpreter, i));
		} else {
			arg = std::move(args[i]);
		}

		switch (arg.type) {
		case Value::Type::Array:
		case Value::Type::Block:
			Try(create_chord(chord, interpreter, std::move(arg)));
			break;

		case Value::Type::Music:
			std::copy_if(arg.chord.notes.begin(), arg.chord.notes.end(), std::back_inserter(chord), [](Note const& n) { return bool(n.base); });
			break;

		default:
			assert(false, "this type is not supported inside chord"); // TODO(assert)
		}
	}

	return {};
}

/// Define handler for members of context that allow reading and writing to them
template<auto Mem_Ptr>
static Result<Value> ctx_read_write_property(Interpreter &interpreter, std::vector<Value> args)
{
	assert(args.size() <= 1, "Ctx get or set is only supported (wrong number of arguments)"); // TODO(assert)

	using Member_Type = std::remove_cvref_t<decltype(std::declval<Context>().*(Mem_Ptr))>;

	if (args.size() == 0) {
		return Value::from(Number(interpreter.context_stack.back().*(Mem_Ptr)));
	}

	assert(args.front().type == Value::Type::Number, "Ctx only holds numeric values");

	if constexpr (std::is_same_v<Member_Type, Number>) {
		interpreter.context_stack.back().*(Mem_Ptr) = args.front().n;
	} else {
		interpreter.context_stack.back().*(Mem_Ptr) = static_cast<Member_Type>(args.front().n.as_int());
	}

	return Value{};
}

/// Iterate over array and it's subarrays to create one flat array
static Result<Array> into_flat_array(Interpreter &i, std::span<Value> args)
{
	Array array;
	for (auto &arg : args) {
		switch (arg.type) {
		case Value::Type::Array:
			std::move(arg.array.elements.begin(), arg.array.elements.end(), std::back_inserter(array.elements));
			break;

		case Value::Type::Block:
			for (auto j = 0u; j < arg.blk.size(); ++j) {
				array.elements.push_back(Try(arg.blk.index(i, j)));
			}
			break;

		default:
			array.elements.push_back(std::move(arg));
		}
	}
	return array;
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
	using N = Shape<Value::Type::Number>;
	if (N::typecheck(args)) {
		return Value::from((std::get<0>(N::move_from(args)).*Method)());
	}

	auto array = Try(into_flat_array(i, std::span(args)));

	for (Value &arg : array.elements) {
		if (arg.type != Value::Type::Number)
			goto invalid_argument_type;
		arg.n = (arg.n.*Method)();
	}
	return Value::from(std::move(array));

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
	using N   = Shape<Value::Type::Number>;
	using NN  = Shape<Value::Type::Number, Value::Type::Number>;
	using NNN = Shape<Value::Type::Number, Value::Type::Number, Value::Type::Number>;

	auto start = Number(0), stop = Number(0), step = Number(1);

	if (0) {}
	else if (auto a = N::typecheck_and_move(args))   { std::tie(stop)              = *a; }
	else if (auto a = NN::typecheck_and_move(args))  { std::tie(start, stop)       = *a; }
	else if (auto a = NNN::typecheck_and_move(args)) { std::tie(start, stop, step) = *a; }
	else {
		return Error {
			.details = errors::Unsupported_Types_For {
				.type = errors::Unsupported_Types_For::Function,
				.name = "range",
				.possibilities = {
					"(stop: number) -> array of number",
					"(start: number, stop: number) -> array of number",
					"(start: number, stop: number, step: number) -> array of number",
				}
			},
			.location = {}
		};
	}

	Array array;
	if constexpr (dir == Range_Direction::Up) {
		for (; start < stop; start += step) {
			array.elements.push_back(Value::from(start));
		}
	} else {
		for (; start < stop; start += step) {
			array.elements.push_back(Value::from(stop - start - Number(1)));
		}
	}
	return Value::from(std::move(array));
}

/// Send MIDI Program Change message
static auto builtin_program_change(Interpreter &i, std::vector<Value> args) -> Result<Value> {
	using Program = Shape<Value::Type::Number>;
	using Channel_Program = Shape<Value::Type::Number, Value::Type::Number>;

	if (Program::typecheck(args)) {
		auto [program] = Program::move_from(args);
		i.midi_connection->send_program_change(0, program.as_int());
		return Value{};
	}

	if (Channel_Program::typecheck(args)) {
		auto [chan, program] = Channel_Program::move_from(args);
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
	switch (v.type) {
	break; case Value::Type::Array:
		for (auto &el : v.array.elements)
			Try(sequential_play(i, std::move(el)));

	break; case Value::Type::Block:
		Try(sequential_play(i, Try(i.eval(std::move(v).blk.body))));

	break; case Value::Type::Music:
		return i.play(v.chord);

	break; default:
		;
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
	Try(ensure_midi_connection_available(i, Midi_Connection_Type::Output, "play"));
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
	Try(ensure_midi_connection_available(i, Midi_Connection_Type::Output, "par"));

	assert(args.size() >= 1, "par only makes sense for at least one argument"); // TODO(assert)
	if (args.size() == 1) {
		Try(i.play(std::move(args.front()).chord));
		return Value{};
	}

	// Create chord that should sustain during playing of all other notes
	auto &ctx = i.context_stack.back();
	auto chord = std::move(args.front()).chord;
	std::for_each(chord.notes.begin(), chord.notes.end(), [&](Note &note) { note = ctx.fill(note); });

	for (auto const& note : chord.notes) {
		if (note.base) {
			i.midi_connection->send_note_on(0, *note.into_midi_note(), 127);
		}
	}

	auto result = builtin_play(i, std::span(args).subspan(1));

	for (auto const& note : chord.notes) {
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
			if (arg.type == Value::Type::Music) {
				track.push_back(std::move(arg).chord);
				return {};
			}

			if (is_indexable(arg.type)) {
				for (auto i = 0u; i < arg.size(); ++i) {
					auto value = Try(arg.index(interpreter, i));
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
	using N = Shape<Value::Type::Number>;

	if (N::typecheck(args)) {
		// Better sieve could be Sieve of Atkin, but it's more complicated
		// so for now we would use Eratosthenes one.
		auto [n_frac] = N::move_from(args);
		if (n_frac.simplify_inplace(); n_frac.num <= 1) {
			return Value::from(Array{});
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
				results.push_back(Value::from(Number(i)));
			}
		}
		return Value::from(Array { .elements = results });
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

	if (args.size() != 2) {
		return guard.yield_error();
	}

	Try(guard(is_indexable, args[0]));
	Try(guard(is_callable, args[1]));

	Value result{};
	for (size_t n = 0; n < args[0].size(); ++n) {
		result = Try(args[1](i, { Try(args[0].index(i, n)) }));
	}
	return result;
}

/// Fold container
static Result<Value> builtin_fold(Interpreter &interpreter, std::vector<Value> args) {
	constexpr auto guard = Guard<2> {
		.name = "fold",
		.possibilities = {
			"(array, callback) -> any",
			"(array, init, callback) -> any"
		}
	};

	Value array, init, callback;
	switch (args.size()) {
	break; case 2:
		array    = std::move(args[0]);
		callback = std::move(args[1]);
		if (array.size() != 0) {
			init = Try(array.index(interpreter, 0));
		}
	break; case 3:
		array    = std::move(args[0]);
		init     = std::move(args[1]);
		callback = std::move(args[2]);
	break; default:
		return guard.yield_error();
	}

	Try(guard(is_indexable, array));
	Try(guard(is_callable, callback));

	for (auto i = 0u; i < array.size(); ++i) {
		auto element = Try(array.index(interpreter, i));
		init = Try(callback(interpreter, { std::move(init), std::move(element) }));
	}

	return init;
}

/// Execute blocks depending on condition
static Result<Value> builtin_if(Interpreter &i, std::vector<Value> args)  {
	constexpr auto guard = Guard<2> {
		.name = "if",
		.possibilities = {
			"(any, function) -> any",
			"(any, function, function) -> any"
		}
	};

	if (args.size() != 2 && args.size() != 3) {
		return guard.yield_error();
	}

	if (args.front().truthy()) {
		Try(guard(is_callable, args[1]));
		return args[1](i, {});
	} else if (args.size() == 3) {
		Try(guard(is_callable, args[2]));
		return args[2](i, {});
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
		Try(guard(is_callable, args[0]));
		return std::move(args[0])(interpreter, {}).value_or(Value{});
	}

	Value success;

	for (usize i = 0; i+1 < args.size(); ++i) {
		Try(guard(is_callable, args[i]));
		if (auto result = std::move(args[i])(interpreter, {})) {
			success = *std::move(result);
		} else {
			Try(guard(is_callable, args.back()));
			return std::move(args.back())(interpreter, {});
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

	using Eager_And_Number = Shape<Value::Type::Array, Value::Type::Number>;
	using Lazy_And_Number  = Shape<Value::Type::Block, Value::Type::Number>;

	if (Eager_And_Number::typecheck_front(args)) {
		auto [v, index] = Eager_And_Number::move_from(args);
		v.elements[index.as_int()] = std::move(args.back());
		return Value::from(std::move(v));
	}

	if (Lazy_And_Number::typecheck_front(args)) {
		auto [v, index] = Lazy_And_Number::move_from(args);
		auto array = Try(flatten(i, { Value::from(std::move(v)) }));
		array[index.as_int()] = std::move(args.back());
		return Value::from(std::move(array));
	}

	return guard.yield_error();
}

/// Return typeof variable
static Result<Value> builtin_typeof(Interpreter&, std::vector<Value> args)
{
	assert(args.size() == 1, "typeof expects only one argument");
	return Value::from(std::string(type_name(args.front().type)));
}

/// Return length of container or set/get default length to play
static Result<Value> builtin_len(Interpreter &i, std::vector<Value> args)
{
	if (args.size() != 1 || !is_indexable(args.front().type)) {
		return ctx_read_write_property<&Context::length>(i, std::move(args));
	}
	return Value::from(Number(args.front().size()));
}

/// Join arguments into flat array
static Result<Value> builtin_flat(Interpreter &i, std::vector<Value> args)
{
	return Value::from(Try(into_flat_array(i, std::move(args))));
}

/// Shuffle arguments
static Result<Value> builtin_shuffle(Interpreter &i, std::vector<Value> args)
{
	static std::mt19937 rnd{std::random_device{}()};
	auto array = Try(flatten(i, std::move(args)));
	std::shuffle(array.begin(), array.end(), rnd);
	return Value::from(std::move(array));
}

/// Permute arguments
static Result<Value> builtin_permute(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	std::next_permutation(array.begin(), array.end());
	return Value::from(std::move(array));
}

/// Sort arguments
static Result<Value> builtin_sort(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	std::sort(array.begin(), array.end());
	return Value::from(std::move(array));
}

/// Reverse arguments
static Result<Value> builtin_reverse(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	std::reverse(array.begin(), array.end());
	return Value::from(std::move(array));
}

/// Get minimum of arguments
static Result<Value> builtin_min(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	auto min = std::min_element(array.begin(), array.end());
	if (min == array.end())
		return Value{};
	return *min;
}

/// Get maximum of arguments
static Result<Value> builtin_max(Interpreter &i, std::vector<Value> args)
{
	auto array = Try(flatten(i, std::move(args)));
	auto max = std::max_element(array.begin(), array.end());
	if (max == array.end())
		return Value{};
	return *max;
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

	auto predicate = std::move(args.front());
	Try(guard(is_callable, predicate));
	auto array = Try(flatten(i, std::span(args).subspan(1)));

	Array tuple[2] = {};
	for (auto &value : array) {
		tuple[Try(predicate(i, { std::move(value) })).truthy()].elements.push_back(std::move(value));
	}

	return Value::from(Array { .elements = {
		Value::from(std::move(tuple[true])),
		Value::from(std::move(tuple[false]))
	}});
}

/// Rotate arguments by n steps
static Result<Value> builtin_rotate(Interpreter &i, std::vector<Value> args)
{
	constexpr auto guard = Guard<1> {
		.name = "rotate",
		.possibilities = { "(number, ...array) -> array" }
	};

	if (args.empty() || args.front().type != Value::Type::Number) {
		return guard.yield_error();
	}

	auto offset = std::move(args.front()).n.as_int();
	auto array = Try(flatten(i, std::span(args).subspan(1)));
	if (offset > 0) {
		offset = offset % array.size();
		std::rotate(array.begin(), array.begin() + offset, array.end());
	} else if (offset < 0) {
		offset = -offset % array.size();
		std::rotate(array.rbegin(), array.rbegin() + offset, array.rend());
	}
	return Value::from(std::move(array));
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
	return Value::from(std::move(result));
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
	return Value::from(std::move(result));
}

static Result<Value> builtin_hash(Interpreter&, std::vector<Value> args)
{
	return Value::from(Number(
		std::accumulate(args.cbegin(), args.cend(), size_t(0), [](size_t h, Value const& v) {
			return hash_combine(h, std::hash<Value>{}(v));
		})
	));
}

/// Build chord from arguments
static Result<Value> builtin_chord(Interpreter &i, std::vector<Value> args)
{
	Chord chord;
	Try(create_chord(chord.notes, i, std::move(args)));
	return Value::from(std::move(chord));
}

/// Send MIDI message Note On
static Result<Value> builtin_note_on(Interpreter &i, std::vector<Value> args)
{
	using Channel_Note_Velocity = Shape<Value::Type::Number, Value::Type::Number, Value::Type::Number>;
	using Channel_Music_Velocity = Shape<Value::Type::Number, Value::Type::Music, Value::Type::Number>;

	if (Channel_Note_Velocity::typecheck(args)) {
		auto [chan, note, vel] = Channel_Note_Velocity::move_from(args);
		i.midi_connection->send_note_on(chan.as_int(), note.as_int(), vel.as_int());
		return Value {};
	}

	if (Channel_Music_Velocity::typecheck(args)) {
		auto [chan, chord, vel] = Channel_Music_Velocity::move_from(args);

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
	using Channel_Note = Shape<Value::Type::Number, Value::Type::Number>;
	using Channel_Music = Shape<Value::Type::Number, Value::Type::Music>;

	if (Channel_Note::typecheck(args)) {
		auto [chan, note] = Channel_Note::move_from(args);
		i.midi_connection->send_note_off(chan.as_int(), note.as_int(), 127);
		return Value {};
	}

	if (Channel_Music::typecheck(args)) {
		auto [chan, chord] = Channel_Music::move_from(args);

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

/// Add handler for incoming midi messages
static Result<Value> builtin_incoming(Interpreter &i, std::vector<Value> args)
{
	if (args.size() != 2 || args[0].type != Value::Type::Symbol || !is_callable(args[1].type)) {
		return Error {
			.details = errors::Unsupported_Types_For {
				.type = errors::Unsupported_Types_For::Function,
				.name = "incoming",
				.possibilities = { "(symbol, function) -> nil" }
			},
			.location = {}
		};
	}

	std::string const& symbol = args[0].s;

	if (symbol == "note_on" || symbol == "noteon") {
		i.callbacks->note_on = std::move(args[1]);
	} else if (symbol == "note_off" || symbol == "noteoff") {
		i.callbacks->note_off = std::move(args[1]);
	} else {

	}
	return Value{};
}

/// Interleaves arguments
static Result<Value> builtin_mix(Interpreter &i, std::vector<Value> args)
{
	std::vector<Value> result;

	std::unordered_map<std::size_t, std::size_t> indicies;

	size_t awaiting_containers = std::count_if(args.begin(), args.end(), [](Value const& v) { return is_indexable(v.type); });

	// Algorithm description:
	// Repeat until all arguments were exhausted:
	//   For each argument:
	//     If it can be iterated then add current element to the list and move to the next one.
	//       If next element cannot be retrived mark container as exhausted and start from beginning
	//     Otherwise append element to the list
	do {
		for (size_t idx = 0; idx < args.size(); ++idx) {
			if (auto &arg = args[idx]; is_indexable(arg.type)) {
				result.push_back(Try(arg.index(i, indicies[idx]++ % arg.size())));
				if (indicies[idx] == arg.size())
					awaiting_containers--;
			} else {
				result.push_back(arg);
			}
		}
	} while (awaiting_containers);

	return Value::from(std::move(result));
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

	if (args.size() == 0) {
		return guard.yield_error();
	}

	auto callable = args.front();
	Try(guard(is_callable, callable));
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
	global.force_define("flat",           builtin_flat);
	global.force_define("floor",          apply_numeric_transform<&Number::floor>);
	global.force_define("fold",           builtin_fold);
	global.force_define("for",            builtin_for);
	global.force_define("hash",           builtin_hash);
	global.force_define("if",             builtin_if);
	global.force_define("incoming",       builtin_incoming);
	global.force_define("instrument",     builtin_program_change);
	global.force_define("len",            builtin_len);
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
	global.force_define("shuffle",        builtin_shuffle);
	global.force_define("sim",            builtin_sim);
	global.force_define("sort",           builtin_sort);
	global.force_define("try",            builtin_try);
	global.force_define("typeof",         builtin_typeof);
	global.force_define("uniq",           builtin_uniq);
	global.force_define("unique",         builtin_unique);
	global.force_define("up",             builtin_range<Range_Direction::Up>);
	global.force_define("update",         builtin_update);
}
