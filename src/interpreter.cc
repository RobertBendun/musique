#include <musique.hh>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

struct Interpreter::Incoming_Midi_Callbacks
{
	Value note_on{};
	Value note_off{};

	Incoming_Midi_Callbacks() = default;

	Incoming_Midi_Callbacks(Incoming_Midi_Callbacks &&) = delete;
	Incoming_Midi_Callbacks(Incoming_Midi_Callbacks const&) = delete;

	Incoming_Midi_Callbacks& operator=(Incoming_Midi_Callbacks &&) = delete;
	Incoming_Midi_Callbacks& operator=(Incoming_Midi_Callbacks const&) = delete;

	void add_callbacks(midi::Connection &midi, Interpreter &interpreter)
	{
		register_callback(midi.note_on_callback, note_on, interpreter);
		register_callback(midi.note_off_callback, note_off, interpreter);
	}

	template<typename ...T>
	void register_callback(std::function<void(T...)> &target, Value &callback, Interpreter &i)
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

void Interpreter::register_callbacks()
{
	assert(callbacks != nullptr, "Interpreter constructor should initialize this field");
	callbacks->add_callbacks(*midi_connection, *this);

	callbacks->note_on = Value(+[](Interpreter &, std::vector<Value> args) -> Result<Value> {
		std::cout << "Received: " << args[1] << "\r\n" << std::flush;
		return Value{};
	});
}

/// Intrinsic implementation primitive providing a short way to check if arguments match required type signature
static inline bool typecheck(std::vector<Value> const& args, auto const& ...expected_types)
{
	return (args.size() == sizeof...(expected_types)) &&
		[&args, expected_types...]<std::size_t ...I>(std::index_sequence<I...>) {
			return ((expected_types == args[I].type) && ...);
		} (std::make_index_sequence<sizeof...(expected_types)>{});
}

/// Intrinsic implementation primitive providing a short way to move values based on matched type signature
static inline bool typecheck_front(std::vector<Value> const& args, auto const& ...expected_types)
{
	return (args.size() >= sizeof...(expected_types)) &&
		[&args, expected_types...]<std::size_t ...I>(std::index_sequence<I...>) {
			return ((expected_types == args[I].type) && ...);
		} (std::make_index_sequence<sizeof...(expected_types)>{});
}

/// Intrinsic implementation primitive providing a short way to move values based on matched type signature
template<auto ...Types>
static inline auto move_from(std::vector<Value>& args)
{
	return [&args]<std::size_t ...I>(std::index_sequence<I...>) {
		return std::tuple { (std::move(args[I]).*(Member_For_Value_Type<Types>::value)) ... };
	} (std::make_index_sequence<sizeof...(Types)>{});
}

/// Shape abstraction to define what types are required once
template<auto ...Types>
struct Shape
{
	static inline auto move_from(std::vector<Value>& args)       { return ::move_from<Types...>(args); }
	static inline auto typecheck(std::vector<Value>& args)       { return ::typecheck(args, Types...); }
	static inline auto typecheck_front(std::vector<Value>& args) { return ::typecheck_front(args, Types...); }
};

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
static bool may_be_vectorized(std::vector<Value> const& args)
{
	return args.size() == 2 && (is_indexable(args[0].type) != is_indexable(args[1].type));
}

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

/// Intrinsic implementation primitive to ease operation vectorization
/// @invariant args.size() == 2
Result<Value> vectorize(auto &&operation, Interpreter &interpreter, std::vector<Value> args)
{
	assert(args.size() == 2, "Vectorization primitive only supports two arguments");
	Array array;

	auto lhs = std::move(args.front());
	auto rhs = std::move(args.back());

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
			note.base = Binary_Operation{}(note.base, offset.as_int());
			note.simplify_inplace();
		}
		return Value::from(std::move(chord));
	}

	if (NM::typecheck(args)) {
		auto [offset, chord] = NM::move_from(args);
		for (auto &note : chord.notes) {
			note.base = Binary_Operation{}(offset.as_int(), note.base);
			note.simplify_inplace();
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

	using NN = Shape<Value::Type::Number, Value::Type::Number>;

	if (NN::typecheck(args)) {
		auto [lhs, rhs] = NN::move_from(args);
		return Value::from(Binary_Operation{}(lhs, rhs));
	}

	if (may_be_vectorized(args)) {
		return vectorize(binary_operator<Binary_Operation, Chars...>, interpreter, args);
	}

	return Error {
		.details = errors::Unsupported_Types_For {
			.type = errors::Unsupported_Types_For::Operator,
			.name = Name,
			.possibilities = {
				"(number, number) -> number",
				"(array, number) -> array",
				"(number, array) -> array"
			}
		},
		.location = {}, // TODO fill location
	};
}

template<typename Binary_Predicate>
static Result<Value> equality_operator(Interpreter&, std::vector<Value> args)
{
	assert(args.size() == 2, "(in)Equality only allows for 2 operands"); // TODO(assert)
	return Value::from(Binary_Predicate{}(std::move(args.front()), std::move(args.back())));
}

template<typename Binary_Predicate>
static Result<Value> comparison_operator(Interpreter&, std::vector<Value> args)
{
	assert(args.size() == 2, "Operator handler cannot accept any shape different then 2 arguments");
	return Value::from(Binary_Predicate{}(args.front(), args.back()));
}

/// Registers constants like `fn = full note = 1/1`
static inline void register_note_length_constants()
{
	auto &global = *Env::global;
	global.force_define("fn",   Value::from(Number(1,  1)));
	global.force_define("dfn",  Value::from(Number(3,  2)));
	global.force_define("hn",   Value::from(Number(1,  2)));
	global.force_define("dhn",  Value::from(Number(3,  4)));
	global.force_define("ddhn", Value::from(Number(7,  8)));
	global.force_define("qn",   Value::from(Number(1,  4)));
	global.force_define("dqn",  Value::from(Number(3,  8)));
	global.force_define("ddqn", Value::from(Number(7, 16)));
	global.force_define("en",   Value::from(Number(1,  8)));
	global.force_define("den",  Value::from(Number(3, 16)));
	global.force_define("dden", Value::from(Number(7, 32)));
	global.force_define("sn",   Value::from(Number(1, 16)));
	global.force_define("dsn",  Value::from(Number(3, 32)));
	global.force_define("tn",   Value::from(Number(1, 32)));
	global.force_define("dtn",  Value::from(Number(3, 64)));
}

template<typename T>
concept With_Index_Method = requires (T t, Interpreter interpreter, usize position) {
	{ t.index(interpreter, position) } -> std::convertible_to<Result<Value>>;
};

template<typename T>
concept With_Index_Operator = requires (T t, unsigned i) {
	{ t[i] } -> std::convertible_to<Value>;
};

template<typename T>
concept Iterable = (With_Index_Method<T> || With_Index_Operator<T>) && requires (T const t) {
	{ t.size() } -> std::convertible_to<usize>;
};

template<Iterable T>
static inline Result<void> create_chord(std::vector<Note> &chord, Interpreter &interpreter, T args)
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
			std::move(arg.chord.notes.begin(), arg.chord.notes.end(), std::back_inserter(chord));
			break;

		default:
			assert(false, "this type is not supported inside chord");
		}
	}

	return {};
}

template<Iterable T>
static inline Result<void> play_notes(Interpreter &interpreter, T args)
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
			Try(play_notes(interpreter, std::move(arg)));
			break;

		case Value::Type::Music:
			interpreter.play(arg.chord);
			break;

		default:
			assert(false, "this type does not support playing");
		}
	}

	return {};
}

template<auto Mem_Ptr>
Result<Value> ctx_read_write_property(Interpreter &interpreter, std::vector<Value> args)
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

Interpreter::Interpreter()
{
	{ // Context initialization
		context_stack.emplace_back();
	}
	{ // Environment initlialization
		assert(!bool(Env::global), "Only one instance of interpreter can be at one time");
		env = Env::global = Env::make();
	}
	{ // MIDI input callbacks initialization
		callbacks = std::make_unique<Interpreter::Incoming_Midi_Callbacks>();
	}
	{ // Global default functions initialization
		auto &global = *Env::global;

		register_note_length_constants();

		global.force_define("update", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			assert(args.size() == 3, "Update requires 3 arguments"); // TODO(assert)
			using Eager_And_Number = Shape<Value::Type::Array, Value::Type::Number>;
			using Lazy_And_Number  = Shape<Value::Type::Block, Value::Type::Number>;

			if (Eager_And_Number::typecheck_front(args)) {
				auto [v, index] = Eager_And_Number::move_from(args);
				v.elements[index.as_int()] = std::move(args.back());
				return Value::from(std::move(v));
			}

			if (Lazy_And_Number::typecheck_front(args)) {
				auto [v, index] = Lazy_And_Number::move_from(args);
				auto array = Try(into_flat_array(i, { Value::from(std::move(v)) }));
				array.elements[index.as_int()] = std::move(args.back());
				return Value::from(std::move(array));
			}

			unimplemented("Wrong shape of update function");
		});

		global.force_define("typeof", +[](Interpreter&, std::vector<Value> args) -> Result<Value> {
			assert(args.size() == 1, "typeof expects only one argument");
			return Value::from(std::string(type_name(args.front().type)));
		});

		global.force_define("if", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			if (args.size() != 2 && args.size() != 3) {
error:
				return Error {
					.details = errors::Unsupported_Types_For {
						.type = errors::Unsupported_Types_For::Function,
						.name = "if",
						.possibilities = {
							"(any, function) -> any",
							"(any, function, function) -> any"
						}
					}
				};
			}
			if (args.front().truthy()) {
				if (not is_callable(args[1].type)) goto error;
				return args[1](i, {});
			} else if (args.size() == 3) {
				if (not is_callable(args[2].type)) goto error;
				return args[2](i, {});
			} else {
				return Value{};
			}
		});

		global.force_define("len", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			if (args.size() != 1 || !is_indexable(args.front().type)) {
				return ctx_read_write_property<&Context::length>(i, std::move(args));
			}
			return Value::from(Number(args.front().size()));
		});

		global.force_define("play", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			Try(play_notes(i, std::move(args)));
			return Value{};
		});

		global.force_define("flat", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			return Value::from(Try(into_flat_array(i, std::move(args))));
		});

		global.force_define("shuffle", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			static std::mt19937 rnd{std::random_device{}()};
			auto array = Try(into_flat_array(i, std::move(args)));
			std::shuffle(array.elements.begin(), array.elements.end(), rnd);
			return Value::from(std::move(array));
		});

		global.force_define("permute", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			auto array = Try(into_flat_array(i, std::move(args)));
			std::next_permutation(array.elements.begin(), array.elements.end());
			return Value::from(std::move(array));
		});

		global.force_define("sort", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			auto array = Try(into_flat_array(i, std::move(args)));
			std::sort(array.elements.begin(), array.elements.end());
			return Value::from(std::move(array));
		});

		global.force_define("reverse", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			auto array = Try(into_flat_array(i, std::move(args)));
			std::reverse(array.elements.begin(), array.elements.end());
			return Value::from(std::move(array));
		});

		global.force_define("min", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			auto array = Try(into_flat_array(i, std::move(args)));
			auto min = std::min_element(array.elements.begin(), array.elements.end());
			if (min == array.elements.end())
				return Value{};
			return *min;
		});

		global.force_define("max", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			auto array = Try(into_flat_array(i, std::move(args)));
			auto max = std::max_element(array.elements.begin(), array.elements.end());
			if (max == array.elements.end())
				return Value{};
			return *max;
		});

		global.force_define("partition", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			assert(!args.empty(), "partition requires function to partition with"); // TODO(assert)
			auto predicate = std::move(args.front());
			assert(is_callable(predicate.type), "partition requires function to partition with"); // TODO(assert)

			auto array = Try(into_flat_array(i, std::span(args).subspan(1)));

			Array tuple[2] = {};
			for (auto &value : array.elements) {
				tuple[Try(predicate(i, { std::move(value) })).truthy()].elements.push_back(std::move(value));
			}

			return Value::from(Array { .elements = {
				Value::from(std::move(tuple[true])),
				Value::from(std::move(tuple[false]))
			}});
		});

		global.force_define("rotate", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			assert(!args.empty(), "rotate requires offset"); // TODO(assert)
			auto offset = std::move(args.front()).n.as_int();
			auto array = Try(into_flat_array(i, std::span(args).subspan(1)));
			if (offset > 0) {
				offset = offset % array.elements.size();
				std::rotate(array.elements.begin(), array.elements.begin() + offset, array.elements.end());
			} else if (offset < 0) {
				offset = -offset % array.elements.size();
				std::rotate(array.elements.rbegin(), array.elements.rbegin() + offset, array.elements.rend());
			}
			return Value::from(std::move(array));
		});

		global.force_define("chord", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			Chord chord;
			Try(create_chord(chord.notes, i, std::move(args)));
			return Value::from(std::move(chord));
		});

		global.force_define("bpm", &ctx_read_write_property<&Context::bpm>);
		global.force_define("oct", &ctx_read_write_property<&Context::octave>);

		global.force_define("par", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			assert(args.size() >= 1, "par only makes sense for at least one argument"); // TODO(assert)
			if (args.size() == 1) {
				i.play(std::move(args.front()).chord);
				return Value{};
			}

			auto &ctx = i.context_stack.back();
			auto chord = std::move(args.front()).chord;
			std::transform(chord.notes.begin(), chord.notes.end(), chord.notes.begin(), [&](Note note) { return ctx.fill(note); });

			for (auto const& note : chord.notes) {
				i.midi_connection->send_note_on(0, *note.into_midi_note(), 127);
			}

			for (auto it = std::next(args.begin()); it != args.end(); ++it) {
				i.play(std::move(*it).chord);
			}

			for (auto const& note : chord.notes) {
				i.midi_connection->send_note_off(0, *note.into_midi_note(), 127);
			}

			return Value{};
		});

		global.force_define("note_on", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
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
		});

		global.force_define("note_off", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
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
		});

		global.force_define("incoming", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
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
		});

		{
			constexpr auto pgmchange = +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
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
						.name = "note_off",
						.possibilities = {
							"(number) -> nil",
							"(number, number) -> nil",
						}
					},
					.location = {}
				};
			};

			global.force_define("instrument",     pgmchange);
			global.force_define("pgmchange",      pgmchange);
			global.force_define("program_change", pgmchange);
		}


		operators["+"] = plus_minus_operator<std::plus<>>;
		operators["-"] = plus_minus_operator<std::minus<>>;
		operators["*"] = binary_operator<std::multiplies<>, '*'>;
		operators["/"] = binary_operator<std::divides<>, '/'>;

		operators["<"]  = comparison_operator<std::less<>>;
		operators[">"]  = comparison_operator<std::greater<>>;
		operators["<="] = comparison_operator<std::less_equal<>>;
		operators[">="] = comparison_operator<std::greater_equal<>>;

		operators["=="] = equality_operator<std::equal_to<>>;
		operators["!="] = equality_operator<std::not_equal_to<>>;

		operators["."] = +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			assert(args.size() == 2, "Operator . requires two arguments"); // TODO(assert)
			assert(args.back().type == Value::Type::Number, "Only numbers can be used for indexing"); // TODO(assert)
			return std::move(args.front()).index(i, std::move(args.back()).n.as_int());
		};
	}
}

Interpreter::~Interpreter()
{
	Env::global.reset();
}

Result<Value> Interpreter::eval(Ast &&ast)
{
	switch (ast.type) {
	case Ast::Type::Literal:
		switch (ast.token.type) {
		case Token::Type::Symbol:
			{
				if (ast.token.source.starts_with('\'')) {
					return Value::from(std::move(ast.token.source).substr(1));
				}

				auto const value = env->find(std::string(ast.token.source));
				assert(value, "Missing variable error is not implemented yet: variable: "s + std::string(ast.token.source));
				return *value;
			}
			return Value{};

		default:
			return Value::from(ast.token);
		}

	case Ast::Type::Binary:
		{
			assert(ast.arguments.size() == 2, "Expected arguments of binary operation to be 2 long");

			if (ast.token.source == "=") {
				auto lhs = std::move(ast.arguments.front());
				auto rhs = std::move(ast.arguments.back());
				assert(lhs.type == Ast::Type::Literal && lhs.token.type == Token::Type::Symbol,
					"Currently LHS of assigment must be an identifier"); // TODO(assert)

				Value *v = env->find(std::string(lhs.token.source));
				assert(v, "Cannot resolve variable: "s + std::string(lhs.token.source)); // TODO(assert)
				return *v = Try(eval(std::move(rhs)));
			}

			if (ast.token.source == "and" || ast.token.source == "or") {
				auto lhs = std::move(ast.arguments.front());
				auto rhs = std::move(ast.arguments.back());

				auto result = Try(eval(std::move(lhs)));
				if (ast.token.source == "or" ? result.truthy() : result.falsy()) {
					return result;
				} else {
					return eval(std::move(rhs));
				}
			}

			auto op = operators.find(std::string(ast.token.source));
			if (op == operators.end()) {
				if (ast.token.source.ends_with('=')) {
					auto op = operators.find(std::string(ast.token.source.substr(0, ast.token.source.size()-1)));
					if (op == operators.end()) {
						return Error {
							.details = errors::Undefined_Operator { .op = ast.token.source },
							.location = ast.token.location
						};
					}

					auto lhs = std::move(ast.arguments.front());
					auto rhs = std::move(ast.arguments.back());
					assert(lhs.type == Ast::Type::Literal && lhs.token.type == Token::Type::Symbol,
						"Currently LHS of assigment must be an identifier"); // TODO(assert)

					Value *v = env->find(std::string(lhs.token.source));
					assert(v, "Cannot resolve variable: "s + std::string(lhs.token.source)); // TODO(assert)
					return *v = Try(op->second(*this, { *v, Try(eval(std::move(rhs))) }));
				}

				return Error {
					.details = errors::Undefined_Operator { .op = ast.token.source },
					.location = ast.token.location
				};
			}

			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : ast.arguments) {
				values.push_back(Try(eval(std::move(a))));
			}

			return op->second(*this, std::move(values));
		}
		break;

	case Ast::Type::Sequence:
		{
			Value v;
			for (auto &a : ast.arguments)
				v = Try(eval(std::move(a)));
			return v;
		}

	case Ast::Type::Call:
		{
			Value func = Try(eval(std::move(ast.arguments.front())));

			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : std::span(ast.arguments).subspan(1)) {
				values.push_back(Try(eval(std::move(a))));
			}
			return std::move(func)(*this, std::move(values));
		}

	case Ast::Type::Variable_Declaration:
		{
			assert(ast.arguments.size() == 2, "Only simple assigments are supported now");
			assert(ast.arguments.front().type == Ast::Type::Literal, "Only names are supported as LHS arguments now");
			assert(ast.arguments.front().token.type == Token::Type::Symbol, "Only names are supported as LHS arguments now");
			env->force_define(std::string(ast.arguments.front().token.source), Try(eval(std::move(ast.arguments.back()))));
			return Value{};
		}

	case Ast::Type::Block:
	case Ast::Type::Lambda:
		{
			Block block;
			if (ast.type == Ast::Type::Lambda) {
				auto parameters = std::span(ast.arguments.begin(), std::prev(ast.arguments.end()));
				block.parameters.reserve(parameters.size());
				for (auto &param : parameters) {
					assert(param.type == Ast::Type::Literal && param.token.type == Token::Type::Symbol, "Not a name in parameter section of Ast::lambda");
					block.parameters.push_back(std::string(std::move(param).token.source));
				}
			}

			block.context = env;
			block.body = std::move(ast.arguments.back());
			return Value::from(std::move(block));
		}

	default:
		unimplemented();
	}
}

void Interpreter::enter_scope()
{
	env = env->enter();
}

void Interpreter::leave_scope()
{
	assert(env != Env::global, "Cannot leave global scope");
	env = env->leave();
}

void Interpreter::play(Chord chord)
{
	assert(midi_connection, "To play midi Interpreter requires instance of MIDI connection");

	auto &ctx = context_stack.back();

	// Fill all notes that don't have octave or length with defaults
	std::transform(chord.notes.begin(), chord.notes.end(), chord.notes.begin(), [&](Note note) { return ctx.fill(note); });

	// Sort that we have smaller times first
	std::sort(chord.notes.begin(), chord.notes.end(), [](Note const& lhs, Note const& rhs) { return lhs.length < rhs.length; });

	Number max_time = *chord.notes.back().length;

	// Turn all notes on
	for (auto const& note : chord.notes) {
		midi_connection->send_note_on(0, *note.into_midi_note(), 127);
	}

	// Turn off each note at right time
	for (auto const& note : chord.notes) {
		if (max_time != Number(0)) {
			max_time -= *note.length;
			std::this_thread::sleep_for(ctx.length_to_duration(*note.length));
		}
		midi_connection->send_note_off(0, *note.into_midi_note(), 127);
	}
}
