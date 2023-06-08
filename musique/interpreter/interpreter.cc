#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>
#include <musique/unicode.hh>

#include <chrono>
#include <iostream>
#include <random>
#include <thread>
#include <condition_variable>
#include <mutex>

extern Result<Value> builtin_prefix_operator_minus(Interpreter&, std::vector<Value>);
extern Result<Value> builtin_prefix_operator_plus(Interpreter&, std::vector<Value>);

std::unordered_map<std::string, Intrinsic> Interpreter::operators {};

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

	// Define note duration symbols from Musical Symbols Unicode block
	{
		Number::value_type pow2 = 1;
		for (u32 rune = 0x1d15d; rune <= 0x1d164; ++rune) {
			global.force_define(utf8::encode(rune), Number(1, pow2));
			pow2 *= 2;
		}
	}

	// Define rests as symbols from Musical Symbols Unicode block
	{
		Number::value_type pow2 = 1;
		for (u32 rune = 0x1d13b; rune <= 0x1d142; ++rune) {
			global.force_define(utf8::encode(rune), Note { .base = std::nullopt, .length = Number(1, pow2) });
			pow2 *= 2;
		}
	}
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

	// Initialize global interpreter state
	random_number_engine.seed(std::random_device{}());
}

Interpreter::~Interpreter()
{
	Env::global.reset();
}

Result<Value> Interpreter::eval(Ast &&ast)
{
	handle_potential_interrupt();

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
						.file = ast.file
					};
				}
				return *value;
			}
			return Value{};

		default:
			return Value::from(ast.file.filename, ast.token);
		}

	break; case Ast::Type::Unary:
	{
		ensure(ast.arguments.size() == 1, "Expected arguments of unary operation to be 1 long");

		std::vector<Value> values;
		values.reserve(ast.arguments.size());
		for (auto& a : ast.arguments) {
			auto const a_loc = a.file;
			values.push_back(Try(eval(std::move(a)).with(a_loc)));
		}
		ensure(ast.token.type == Token::Type::Operator, "Unary operation expects its token to be an operator");

		Result<Value> result;
		if (ast.token.source == "+") {
			result = builtin_prefix_operator_plus(*this, std::move(values));
		} else if (ast.token.source == "-") {
			result = builtin_prefix_operator_minus(*this, std::move(values));
		} else {
			unreachable();
		}

		// TODO Is this good range assigment
		return std::move(result).with(ast.token.location(ast.file.filename));
	}

	break; case Ast::Type::Binary:
		{
			ensure(ast.arguments.size() == 2, "Expected arguments of binary operation to be 2 long");

			if (ast.token.source == "=") {
				auto lhs = std::move(ast.arguments.front());
				auto rhs = std::move(ast.arguments.back());

				if (lhs.type != Ast::Type::Literal || lhs.token.type != Token::Type::Symbol) {
					return Error {
						.details = errors::Unsupported_Types_For {
							.type = errors::Unsupported_Types_For::Operator,
							.name = "=",
							.possibilities = {},
						},
						.file = ast.token.location(ast.file.filename)
					};
				}

				Value *v = env->find(std::string(lhs.token.source));
				if (v == nullptr) {
					return Error {
						.details = errors::Missing_Variable {
							.name = std::string(lhs.token.source)
						},
						.file = lhs.file,
					};
				}
				return *v = Try(eval(std::move(rhs)).with(ast.file));
			}

			if (ast.token.source == "and" || ast.token.source == "or") {
				auto lhs = std::move(ast.arguments.front());
				auto rhs = std::move(ast.arguments.back());

				auto lhs_loc = lhs.file, rhs_loc = rhs.file;

				auto result = Try(eval(std::move(lhs)).with(std::move(lhs_loc)));
				if (ast.token.source == "or" ? result.truthy() : result.falsy()) {
					return result;
				} else {
					return eval(std::move(rhs)).with(rhs_loc);
				}
			}

			auto op = operators.find(std::string(ast.token.source));
			if (op == operators.end()) {
				if (ast.token.source.ends_with('=')) {
					auto op = operators.find(std::string(ast.token.source.substr(0, ast.token.source.size()-1)));
					if (op == operators.end()) {
						return Error {
							.details = errors::Undefined_Operator { .op = std::string(ast.token.source) },
							.file = ast.token.location(ast.file.filename)
						};
					}

					auto lhs = std::move(ast.arguments.front());
					auto rhs = std::move(ast.arguments.back());
					auto const rhs_loc = rhs.file;
					ensure(lhs.type == Ast::Type::Literal && lhs.token.type == Token::Type::Symbol,
						"Currently LHS of assigment must be an identifier"); // TODO(assert)

					Value *v = env->find(std::string(lhs.token.source));
					ensure(v, "Cannot resolve variable: "s + std::string(lhs.token.source)); // TODO(assert)
					return *v = Try(op->second(*this, {
						*v, Try(eval(std::move(rhs)).with(rhs_loc))
					}).with(ast.token.location(ast.file.filename)));
				}

				return Error {
					.details = errors::Undefined_Operator { .op = std::string(ast.token.source) },
					.file = ast.token.location(ast.file.filename)
				};
			}

			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : ast.arguments) {
				auto const a_loc = a.file;
				values.push_back(Try(eval(std::move(a)).with(a_loc)));
			}

			return op->second(*this, std::move(values)).with(ast.token.location(ast.file.filename));
		}
		break;

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
			auto call_location = ast.arguments.front().file;
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
				.with(std::move(call_location));
		}

	case Ast::Type::Variable_Declaration:
		{
			ensure(ast.arguments.size() == 2, "Only simple assigments are supported now");
			ensure(ast.arguments.front().type == Ast::Type::Literal, "Only names are supported as LHS arguments now");
			ensure(ast.arguments.front().token.type == Token::Type::Symbol, "Only names are supported as LHS arguments now");
			env->force_define(std::string(ast.arguments.front().token.source), Try(eval(std::move(ast.arguments.back()))));
			return Value{};
		}

	case Ast::Type::If:
		{
			auto const condition = Try(eval(std::move(ast.arguments.front())));
			if (condition.truthy()) {
				return eval(std::move(ast.arguments[1]));
			} else if (ast.arguments.size() == 3) {
				return eval(std::move(ast.arguments[2]));
			} else {
				return Value{};
			}
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

	handle_potential_interrupt();

	if (chord.notes.size() == 0) {
		sleep(ctx.length_to_duration(ctx.length));
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
			current_context->port->send_note_on(0, *note.into_midi_note(), 127);
			active_notes.emplace(0, *note.into_midi_note());
		}
	}

	// Turn off each note at right time
	for (auto const& note : chord.notes) {
		if (max_time != Number(0)) {
			max_time -= *note.length;
			sleep(ctx.length_to_duration(*note.length));
		}
		if (note.base) {
			current_context->port->send_note_off(0, *note.into_midi_note(), 127);
			active_notes.erase(active_notes.lower_bound(std::pair<unsigned, unsigned>{0, *note.into_midi_note()}));
		}
	}

	return {};
}

void Interpreter::turn_off_all_active_notes()
{
	auto status = ensure_midi_connection_available(*this, "turn_off_all_active_notes");
	if (!Try_Traits<decltype(status)>::is_ok(status)) {
		return;
	}

	// TODO send to port that send_note_on was called on
	for (auto [chan, note] : active_notes) {
		current_context->port->send_note_off(chan, note, 0);
	}

	active_notes.clear();
}


std::optional<Error> ensure_midi_connection_available(Interpreter &interpreter, std::string_view operation_name)
{
	if (interpreter.current_context->port == nullptr || !interpreter.current_context->port->supports_output()) {
		return Error {
			.details = errors::Operation_Requires_Midi_Connection {
				.is_input = false,
				.name = std::string(operation_name),
			},
			.file = {}
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
	break; case Ast::Type::Unary: case Ast::Type::If:
		unimplemented();

	break; case Ast::Type::Sequence:
	{
		for (auto const& a : ast.arguments) {
			snapshot(out, a);
			out << ", ";
		}
	}
	break; case Ast::Type::Block:
		ensure(ast.arguments.size() == 1, "Block can contain only one node which contains its body");
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
		[](Macro const&) { unreachable(); }
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

// TODO This only supports single-threaded interpreter execution
static std::atomic<bool> interrupted = false;
static std::condition_variable condvar;
static std::mutex mu;

void Interpreter::handle_potential_interrupt()
{
	if (interrupted) {
		interrupted = false;
		throw KeyboardInterrupt{};
	}
}

void Interpreter::issue_interrupt()
{
	interrupted = true;
	condvar.notify_all();
}

void Interpreter::sleep(std::chrono::duration<float> time)
{
	if (std::unique_lock lock(mu); condvar.wait_for(lock, time) == std::cv_status::no_timeout) {
		ensure(interrupted, "Only interruption can result in quiting conditional variable without timeout");
		interrupted = false;
		throw KeyboardInterrupt{};
	}
}
