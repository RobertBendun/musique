#include <musique.hh>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

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

Interpreter::Interpreter()
{
	// Context initialization
	context_stack.emplace_back();

	// Environment initlialization
	assert(!bool(Env::global), "Only one instance of interpreter can be at one time");
	env = Env::global = Env::make();

	// Builtins initialization
	register_note_length_constants();
	register_builtin_operators();
	register_builtin_functions();
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

				auto name = std::string(ast.token.source);

				auto const value = env->find(name);
				if (!value) {
					return Error {
						.details  = errors::Missing_Variable { .name = std::move(name) },
						.location = ast.location
					};
				}
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
