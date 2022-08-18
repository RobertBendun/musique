#include <musique.hh>
#include <musique_internal.hh>

#include <random>
#include <memory>
#include <iostream>


void Interpreter::register_callbacks()
{
	assert(callbacks == nullptr, "This field should be uninitialized");
	callbacks = std::make_unique<Interpreter::Incoming_Midi_Callbacks>();
	callbacks->add_callbacks(*midi_connection, *this);

	callbacks->note_on = Value(+[](Interpreter &, std::vector<Value> args) -> Result<Value> {
		std::cout << "Received: " << args[1] << "\r\n" << std::flush;
		return Value{};
	});
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

void Interpreter::register_builtin_functions()
{
	auto &global = *Env::global;

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
}
