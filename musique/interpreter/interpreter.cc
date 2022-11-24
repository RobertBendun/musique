#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <future>
#include <mutex>

midi::Connection *Interpreter::midi_connection = nullptr;
std::unordered_map<std::string, Intrinsic> Interpreter::operators {};

Interpreter Interpreter::clone() const
{
	Interpreter interpreter(Clone{});
	interpreter.default_action = default_action;
	interpreter.env = env->enter();
	interpreter.current_context = std::make_shared<Context>(*current_context);
	interpreter.current_context->parent = current_context;
	return interpreter;
}

/// Registers constants like `fn = full note = 1/1`
static inline void register_note_length_constants()
{
	auto &global = *Env::global;
	global.force_define("wn",   Number(1,  1));
	global.force_define("fn",   Number(1,  1));
	global.force_define("dwn",  Number(3,  2));
	global.force_define("hn",   Number(1,  2));
	global.force_define("dhn",  Number(3,  4));
	global.force_define("ddhn", Number(7,  8));
	global.force_define("qn",   Number(1,  4));
	global.force_define("dqn",  Number(3,  8));
	global.force_define("ddqn", Number(7, 16));
	global.force_define("en",   Number(1,  8));
	global.force_define("den",  Number(3, 16));
	global.force_define("dden", Number(7, 32));
	global.force_define("sn",   Number(1, 16));
	global.force_define("dsn",  Number(3, 32));
	global.force_define("tn",   Number(1, 32));
	global.force_define("dtn",  Number(3, 64));
}

Interpreter::Interpreter()
{
	// Context initialization
	current_context = std::make_shared<Context>();

	// Environment initlialization
	ensure(!bool(Env::global), "Only one instance of interpreter can be at one time");
	env = Env::global = Env::make();

	// Builtins initialization
	register_note_length_constants();
	register_builtin_operators();
	register_builtin_functions();
}

Interpreter::Interpreter(Interpreter::Clone)
{
}

static Result<Value> eval_concurrent(Interpreter &interpreter, Ast &&ast)
{
	ensure(ast.type == Ast::Type::Concurrent, "Only conccurent AST nodes can be evaluated in eval_concurrent");

	std::span<Ast> jobs = ast.arguments;
	std::vector<std::future<Value>> futures;
	std::optional<Error> error;
	std::mutex mutex;

	for (unsigned i = 0; i < jobs.size(); ++i) {
		futures.push_back(std::async(std::launch::async, [interpreter = interpreter.clone(), i, jobs, &mutex, &error]() mutable -> Value {
			auto result = interpreter.eval(std::move(jobs[i]));
			if (result) {
				if (interpreter.default_action) {
					// TODO Code duplication between this section and last section in this lambda function
					if (auto produced_error = interpreter.default_action(interpreter, *std::move(result))) {
						std::lock_guard guard{mutex};
						if (!error) {
							error = produced_error;
						}
						return Value{};
					}
				}
				return *std::move(result);
			}

			std::lock_guard guard{mutex};
			if (!error) {
				error = result.error();
			}
			return Value{};
		}));
	}

	std::vector<Value> results;
	for (auto& future : futures) {
		if (error) {
			return *error;
		}
		results.push_back(future.get());
	}
	return results;
}

Result<Value> Interpreter::eval(Ast &&ast)
{
	switch (ast.type) {
	case Ast::Type::Literal:
		switch (ast.token.type) {
		case Token::Type::Symbol:
			{
				if (ast.token.source.starts_with('\'')) {
					if (auto op = operators.find(std::string(ast.token.source.substr(1))); op != operators.end()) {
						return Value(op->second);
					} else {
						return std::move(ast.token.source).substr(1);
					}
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
			ensure(ast.arguments.size() == 2, "Expected arguments of binary operation to be 2 long");

			if (ast.token.source == "=") {
				auto lhs = std::move(ast.arguments.front());
				auto rhs = std::move(ast.arguments.back());
				ensure(lhs.type == Ast::Type::Literal && lhs.token.type == Token::Type::Symbol,
					"Currently LHS of assigment must be an identifier"); // TODO(assert)

				Value *v = env->find(std::string(lhs.token.source));
				ensure(v, "Cannot resolve variable: "s + std::string(lhs.token.source)); // TODO(assert)
				return *v = Try(eval(std::move(rhs)).with_location(ast.token.location));
			}

			if (ast.token.source == "and" || ast.token.source == "or") {
				auto lhs = std::move(ast.arguments.front());
				auto rhs = std::move(ast.arguments.back());

				auto lhs_loc = lhs.location, rhs_loc = rhs.location;

				auto result = Try(eval(std::move(lhs)).with_location(std::move(lhs_loc)));
				if (ast.token.source == "or" ? result.truthy() : result.falsy()) {
					return result;
				} else {
					return eval(std::move(rhs)).with_location(rhs_loc);
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
					auto const rhs_loc = rhs.location;
					ensure(lhs.type == Ast::Type::Literal && lhs.token.type == Token::Type::Symbol,
						"Currently LHS of assigment must be an identifier"); // TODO(assert)

					Value *v = env->find(std::string(lhs.token.source));
					ensure(v, "Cannot resolve variable: "s + std::string(lhs.token.source)); // TODO(assert)
					return *v = Try(op->second(*this, {
						*v, Try(eval(std::move(rhs)).with_location(rhs_loc))
					}).with_location(ast.token.location));
				}

				return Error {
					.details = errors::Undefined_Operator { .op = ast.token.source },
					.location = ast.token.location
				};
			}

			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : ast.arguments) {
				auto const a_loc = a.location;
				values.push_back(Try(eval(std::move(a)).with_location(a_loc)));
			}

			return op->second(*this, std::move(values)).with_location(ast.token.location);
		}
		break;

	case Ast::Type::Concurrent:
		return eval_concurrent(*this, std::move(ast));

	case Ast::Type::Sequence:
		{
			Value v;
			bool first = true;
			for (auto &a : ast.arguments) {
				if (!first && default_action) Try(default_action(*this, v));
				v = Try(eval(std::move(a)));
				first = false;
			}
			return v;
		}

	case Ast::Type::Call:
		{
			auto call_location = ast.arguments.front().location;
			Value func = Try(eval(std::move(ast.arguments.front())));

			if (auto macro = std::get_if<Macro>(&func.data)) {
				return (*macro)(*this, std::span(ast.arguments).subspan(1));
			}

			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : std::span(ast.arguments).subspan(1)) {
				values.push_back(Try(eval(std::move(a))));
			}
			return std::move(func)(*this, std::move(values))
				.with_location(std::move(call_location));
		}

	case Ast::Type::Variable_Declaration:
		{
			ensure(ast.arguments.size() == 2, "Only simple assigments are supported now");
			ensure(ast.arguments.front().type == Ast::Type::Literal, "Only names are supported as LHS arguments now");
			ensure(ast.arguments.front().token.type == Token::Type::Symbol, "Only names are supported as LHS arguments now");
			env->force_define(std::string(ast.arguments.front().token.source), Try(eval(std::move(ast.arguments.back()))));
			return Value{};
		}

	case Ast::Type::Block:
	case Ast::Type::Lambda:
		{
			Block block;
			if (ast.type == Ast::Type::Lambda) {
				auto parameters = std::span<Ast>(ast.arguments.data(), ast.arguments.size() - 1);
				block.parameters.reserve(parameters.size());
				for (auto &param : parameters) {
					ensure(param.type == Ast::Type::Literal && param.token.type == Token::Type::Symbol, "Not a name in parameter section of Ast::lambda");
					block.parameters.push_back(std::string(std::move(param).token.source));
				}
			}

			block.context = env;
			block.body = std::move(ast.arguments.back());
			return block;
		}
	}

	std::cout << ast.type << std::endl;
	std::cout << ast.token.type << std::endl;
	unreachable();
}

void Interpreter::enter_scope()
{
	env = env->enter();
}

void Interpreter::leave_scope()
{
	ensure(env != Env::global, "Cannot leave global scope");
	env = env->leave();
}

std::optional<Error> Interpreter::play(Chord chord)
{
	Try(ensure_midi_connection_available(*this, "play"));
	auto &ctx = *current_context;

	if (chord.notes.size() == 0) {
		return {};
	}

	// Fill all notes that don't have octave or length with defaults
	std::transform(chord.notes.begin(), chord.notes.end(), chord.notes.begin(), [&](Note note) { return ctx.fill(note); });

	// Sort that we have smaller times first
	std::sort(chord.notes.begin(), chord.notes.end(), [](Note const& lhs, Note const& rhs) { return lhs.length < rhs.length; });

	Number max_time = *chord.notes.back().length;

	// Turn all notes on
	for (auto const& note : chord.notes) {
		if (note.base) {
			midi_connection->send_note_on(0, *note.into_midi_note(), 127);
		}
	}

	// Turn off each note at right time
	for (auto const& note : chord.notes) {
		if (max_time != Number(0)) {
			max_time -= *note.length;
			std::this_thread::sleep_for(ctx.length_to_duration(*note.length));
		}
		if (note.base) {
			midi_connection->send_note_off(0, *note.into_midi_note(), 127);
		}
	}

	return {};
}

std::optional<Error> ensure_midi_connection_available(Interpreter &i, std::string_view operation_name)
{
	if (i.midi_connection == nullptr || !i.midi_connection->supports_output()) {
		return Error {
			.details = errors::Operation_Requires_Midi_Connection {
				.is_input = false,
				.name = std::string(operation_name),
			},
			.location = {}
		};
	}
	return {};
}

static void snapshot(std::ostream& out, Note const& note) {
	if (note.length) {
		out << "(" << note << ")";
	} else {
		out << note;
	}
}

static void snapshot(std::ostream &out, Ast const& ast) {
	switch (ast.type) {
	break;
	case Ast::Type::Sequence:
	case Ast::Type::Concurrent:
	{
		for (auto const& a : ast.arguments) {
			snapshot(out, a);
			out << ", ";
		}
	}
	break; case Ast::Type::Block:
		// TODO
		ensure(ast.arguments.size() == 1, "Block can contain only one node which contains its body");
		if (ast.arguments.front().type == Ast::Type::Concurrent)
			unimplemented("Concurrent code snapshoting is not supported yet");
		out << "(";
		snapshot(out, ast.arguments.front());
		out << ")";

	break; case Ast::Type::Lambda:
		out << "(";
		for (auto i = 0u; i+1 < ast.arguments.size(); ++i) {
			ensure(ast.arguments[i].type == Ast::Type::Literal, "Lambda arguments should be an identifiers");
			out << ast.arguments[i].token.source << " ";
		}
		out << "|";
		snapshot(out, ast.arguments.back());
		out << ")";

	break; case Ast::Type::Variable_Declaration:
		ensure(ast.arguments.size() == 2, "Variable declaration snapshots only support single lhs variables");
		ensure(ast.arguments.front().type == Ast::Type::Literal, "Expected first value to be an identifier");
		out << ast.arguments[0].token.source << " := ";
		return snapshot(out, ast.arguments.back());

	break; case Ast::Type::Literal:
		out << ast.token.source;

	break; case Ast::Type::Binary:
		out << "(";
		snapshot(out, ast.arguments[0]);
		out << ") ";
		out << ast.token.source;
		out << " (";
		snapshot(out, ast.arguments[1]);
		out << ")";

	break; case Ast::Type::Call:
		out << "(";
		for (auto const& a : ast.arguments) {
			out << "(";
			snapshot(out, a);
			out << ") ";
		}
		out << ")";
	}
}

static void snapshot(std::ostream& out, Value const& value) {
	std::visit(Overloaded{
		[&](Nil) { out << "nil"; },
		[&](Bool const& b) {
			out << (b ? "true" : "false");
		},
		[&](Number const& n) {
			out << "(" << n.num << "/" << n.den << ")";
		},
		[&](Array const& array) {
			out << "(flat (";
			for (auto const& nested : array.elements) {
				snapshot(out, nested);
				out << ", ";
			}
			out << "))";
		},
		[&](Chord const& chord) {
			if (chord.notes.size() == 1) {
				auto note = chord.notes.front();
				snapshot(out, note);
			} else {
				out << "(chord ";
				for (auto const &note : chord.notes) {
					snapshot(out, note);
					out << " ";
				}
				out << ")";
			}
		},
		[&](Symbol const& symbol) {
			out << "'" << symbol;
		},
		[&](Block const& block) {
			out << "(";
			for (auto const& param : block.parameters) {
				out << param << ' ';
			}
			out << "| ";
			snapshot(out, block.body);
			out << ")";
		},
		[](Intrinsic const&) { unreachable(); },
		[](Macro const&) { unreachable(); },
		[](Set const&) { unimplemented("Snapshoting is not supported yet"); }
	}, value.data);
}

void Interpreter::snapshot(std::ostream& out)
{
	auto const& ctx = *current_context;
	out << ", oct " << int(ctx.octave) << '\n';
	out << ", len (" << ctx.length.num << "/" << ctx.length.den << ")\n";
	out << ", bpm " << ctx.bpm << '\n';

	for (auto current = env.get(); current; current = current->parent.get()) {
		for (auto const& [name, value] : current->variables) {
			if (std::holds_alternative<Intrinsic>(value.data) || std::holds_alternative<Macro>(value.data)) {
				continue;
			}
			out << ", " << name << " := ";
			::snapshot(out, value);
			out << '\n';
		}
	}
	out << std::flush;
}
