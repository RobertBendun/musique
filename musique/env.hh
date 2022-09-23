#ifndef MUSIQUE_ENV_HH
#define MUSIQUE_ENV_HH

#include <memory>
#include <unordered_map>
#include "value.hh"

/// Collection holding all variables in given scope.
struct Env : std::enable_shared_from_this<Env>
{
	/// Constructor of Env class
	static std::shared_ptr<Env> make();

	/// Global scope that is beeing set by Interpreter
	static std::shared_ptr<Env> global;

	/// Variables in current scope
	std::unordered_map<std::string, Value> variables;

	/// Parent scope
	std::shared_ptr<Env> parent;

	Env(Env const&) = delete;
	Env(Env &&) = default;
	Env& operator=(Env const&) = delete;
	Env& operator=(Env &&) = default;

	/// Defines new variable regardless of it's current existance
	Env& force_define(std::string name, Value new_value);

	/// Finds variable in current or parent scopes
	Value* find(std::string const& name);

	/// Create new scope with self as parent
	std::shared_ptr<Env> enter();

	/// Leave current scope returning parent
	std::shared_ptr<Env> leave();

private:
	/// Ensure that all values of this class are behind shared_ptr
	Env() = default;
};

#endif
