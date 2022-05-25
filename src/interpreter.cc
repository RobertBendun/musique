#include <musique.hh>

#include <chrono>
#include <iostream>
#include <thread>

template<typename Binary_Operation>
constexpr auto binary_operator()
{
	return [](Interpreter&, std::vector<Value> args) -> Result<Value> {
		auto result = std::move(args.front());
		for (auto &v : std::span(args).subspan(1)) {
			assert(result.type == Value::Type::Number, "LHS should be a number"); // TODO(assert)
			assert(v.type == Value::Type::Number,      "RHS should be a number"); // TODO(assert)
			if constexpr (std::is_same_v<Number, std::invoke_result_t<Binary_Operation, Number, Number>>) {
				result.n = Binary_Operation{}(std::move(result.n), std::move(v).n);
			} else {
				result.type = Value::Type::Bool;
				result.b = Binary_Operation{}(std::move(result.n), std::move(v).n);
			}
		}
		return result;
	};
}

template<typename Binary_Predicate>
constexpr auto equality_operator()
{
	return [](Interpreter&, std::vector<Value> args) -> Result<Value> {
		assert(args.size() == 2, "(in)Equality only allows for 2 operands"); // TODO(assert)
		return Value::from(Binary_Predicate{}(std::move(args.front()), std::move(args.back())));
	};
}

template<typename Binary_Predicate>
constexpr auto comparison_operator()
{
	return [](Interpreter&, std::vector<Value> args) -> Result<Value> {
		assert(args.size() == 2, "Ordering only allows for 2 operands"); // TODO(assert)
		assert(args.front().type == args.back().type, "Only values of the same type can be ordered"); // TODO(assert)

		switch (args.front().type) {
		case Value::Type::Number:
			return Value::from(Binary_Predicate{}(std::move(args.front()).n, std::move(args.back()).n));

		case Value::Type::Bool:
			return Value::from(Binary_Predicate{}(std::move(args.front()).b, std::move(args.back()).b));

		default:
			assert(false, "Cannot compare value of given types"); // TODO(assert)
		}
		unreachable();
	};
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

/// Creates implementation of plus/minus operator that support following operations:
///   number, number -> number (standard math operations)
///   n: number, m: music  -> music
///   m: music,  n: number -> music  moves m by n semitones (+ goes up, - goes down)
template<typename Binary_Operation>
[[gnu::always_inline]]
static inline auto plus_minus_operator()
{
	return [](Interpreter&, std::vector<Value> args) -> Result<Value> {
		assert(args.size() == 2, "Binary operator only accepts 2 arguments");
		auto lhs = std::move(args.front());
		auto rhs = std::move(args.back());

		if (lhs.type == rhs.type && lhs.type == Value::Type::Number) {
			return Value::from(Binary_Operation{}(std::move(lhs).n, std::move(rhs).n));
		}

		if (lhs.type == Value::Type::Music && rhs.type == Value::Type::Number) {
music_number_operation:
			for (auto &note : lhs.chord.notes) {
				note.base = Binary_Operation{}(note.base, rhs.n.as_int());
				note.simplify_inplace();
			}
			return lhs;
		}

		if (lhs.type == Value::Type::Number && rhs.type == Value::Type::Music) {
			std::swap(lhs, rhs);
			goto music_number_operation;
		}

		assert(false, "Unsupported types for this operation"); // TODO(assert)
		unreachable();
	};
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
	{ // Global default functions initialization
		auto &global = *Env::global;

		register_note_length_constants();

		global.force_define("typeof", +[](Interpreter&, std::vector<Value> args) -> Result<Value> {
			assert(args.size() == 1, "typeof expects only one argument");
			return Value::from(std::string(type_name(args.front().type)));
		});

		global.force_define("if", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			assert(args.size() == 2 || args.size() == 3, "argument count does not add up - expected: if <condition> <then> [<else>]");
			if (args.front().truthy()) {
				return args[1](i, {});
			} else if (args.size() == 3) {
				return args[2](i, {});
			} else {
				return Value{};
			}
		});

		global.force_define("len", +[](Interpreter &, std::vector<Value> args) -> Result<Value> {
			assert(args.size() == 1, "len only accepts one argument");
			assert(args.front().type == Value::Type::Block || args.front().type == Value::Type::Array, "Only blocks and arrays can have length");
			return Value::from(Number(args.front().size()));
		});

		global.force_define("play", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			for (auto &arg : args) {
				assert(arg.type == Value::Type::Music, "Only music values can be played"); // TODO(assert)
				i.play(arg.chord);
			}
			return Value{};
		});

		global.force_define("flat", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
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
			return Value::from(std::move(array));
		});

		global.force_define("chord", +[](Interpreter &i, std::vector<Value> args) -> Result<Value> {
			Chord chord;
			Try(create_chord(chord.notes, i, std::move(args)));
			return Value::from(std::move(chord));
		});

		operators["+"] = plus_minus_operator<std::plus<>>();
		operators["-"] = plus_minus_operator<std::minus<>>();
		operators["*"] = binary_operator<std::multiplies<>>();
		operators["/"] = binary_operator<std::divides<>>();

		operators["<"]  = comparison_operator<std::less<>>();
		operators[">"]  = comparison_operator<std::greater<>>();
		operators["<="] = comparison_operator<std::less_equal<>>();
		operators[">="] = comparison_operator<std::greater_equal<>>();

		operators["=="] = equality_operator<std::equal_to<>>();
		operators["!="] = equality_operator<std::not_equal_to<>>();

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
			std::vector<Value> values;
			values.reserve(ast.arguments.size());

			auto op = operators.find(std::string(ast.token.source));

			if (op == operators.end()) {
				return errors::unresolved_operator(ast.token);
			}

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
