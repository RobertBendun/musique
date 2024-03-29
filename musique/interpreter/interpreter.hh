#ifndef MUSIQUE_INTERPRETER_HH
#define MUSIQUE_INTERPRETER_HH

#include <musique/interpreter/context.hh>
#include <musique/interpreter/starter.hh>
#include <musique/midi/midi.hh>
#include <musique/value/value.hh>
#include <unordered_map>
#include <set>
#include <random>

struct KeyboardInterrupt : std::exception
{
	~KeyboardInterrupt() = default;
	char const* what() const noexcept override { return "KeyboardInterrupt"; }
};

/// Given program tree evaluates it into Value
struct Interpreter
{
	/// Operators defined for language
	static std::unordered_map<std::string, Intrinsic> operators;

	/// Current environment (current scope)
	std::shared_ptr<Env> env;

	/// Context stack. `constext_stack.back()` is a current context.
	/// There is always at least one context
	std::shared_ptr<Context> current_context;

	std::function<std::optional<Error>(Interpreter&, Value)> default_action;

	std::multiset<std::pair<unsigned, unsigned>> active_notes;

	Starter starter;

	std::mt19937 random_number_engine;

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

	/// Turn all notes that have been played but don't finished playing
	void turn_off_all_active_notes();

	/// Handles interrupt if any occured
	void handle_potential_interrupt();

	/// Issue new interrupt
	void issue_interrupt();

	/// Sleep for at least given time or until interrupt
	void sleep(std::chrono::duration<float>);
};

std::optional<Error> ensure_midi_connection_available(Interpreter&, std::string_view operation_name);

#endif
