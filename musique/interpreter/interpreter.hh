#ifndef MUSIQUE_INTERPRETER_HH
#define MUSIQUE_INTERPRETER_HH

#include <musique/midi/midi.hh>
#include <musique/interpreter/context.hh>
#include <musique/value/value.hh>
#include <unordered_map>

/// Given program tree evaluates it into Value
struct Interpreter
{
	/// MIDI connection that is used to play music.
	/// It's optional for simple interpreter testing.
	midi::Connection *midi_connection = nullptr;

	/// Operators defined for language
	std::unordered_map<std::string, Intrinsic> operators;

	/// Current environment (current scope)
	std::shared_ptr<Env> env;

	/// Context stack. `constext_stack.back()` is a current context.
	/// There is always at least one context
	std::vector<Context> context_stack;

	std::function<std::optional<Error>(Interpreter&, Value)> default_action;

	struct Incoming_Midi_Callbacks;
	std::unique_ptr<Incoming_Midi_Callbacks> callbacks;
	void register_callbacks();

	Interpreter();
	~Interpreter();
	Interpreter(Interpreter const&) = delete;
	Interpreter(Interpreter &&) = default;

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
};

enum class Midi_Connection_Type { Output, Input };
std::optional<Error> ensure_midi_connection_available(Interpreter&, Midi_Connection_Type, std::string_view operation_name);

#endif
