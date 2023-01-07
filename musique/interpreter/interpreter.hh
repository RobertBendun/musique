#ifndef MUSIQUE_INTERPRETER_HH
#define MUSIQUE_INTERPRETER_HH

#include <musique/midi/midi.hh>
#include <musique/interpreter/context.hh>
#include <musique/value/value.hh>
#include <unordered_map>
#include <musique/serialport/serialport.hh>

/// Given program tree evaluates it into Value
struct Interpreter
{
	/// MIDI connection that is used to play music.
	/// It's optional for simple interpreter testing.
	static midi::Connection *midi_connection;

	/// Operators defined for language
	static std::unordered_map<std::string, Intrinsic> operators;

	/// Current environment (current scope)
	std::shared_ptr<Env> env;

	/// Context stack. `constext_stack.back()` is a current context.
	/// There is always at least one context
	std::shared_ptr<Context> current_context;

	std::function<std::optional<Error>(Interpreter&, Value)> default_action;

	std::shared_ptr<serialport::State> serialport;

	Interpreter();
	~Interpreter();
	Interpreter(Interpreter &&) = delete;
	Interpreter(Interpreter const&) = delete;

	/// Try to evaluate given program tree
	Result<Value> eval(Ast &&ast);

	// Enter scope by changing current environment
	void enter_scope();

	// Leave scope by changing current environment
	void leave_scope();

	/// Play note resolving any missing parameters with context via `midi_connection` member.
	std::optional<Error> play(Chord);

	/// Add to global interpreter scope all builtin function definitions
	///
	/// Invoked during construction
	void register_builtin_functions();

	/// Add to interpreter operators table all operators
	///
	/// Invoked during construction
	void register_builtin_operators();

	/// Dumps snapshot of interpreter into stream
	void snapshot(std::ostream& out);
};

std::optional<Error> ensure_midi_connection_available(Interpreter&, std::string_view operation_name);

#endif
