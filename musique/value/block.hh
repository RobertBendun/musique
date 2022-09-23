#ifndef MUSIQUE_BLOCK_HH
#define MUSIQUE_BLOCK_HH

#include <musique/result.hh>
#include <musique/parser/ast.hh>

#include <memory>

struct Env;
struct Interpreter;
struct Value;

using Intrinsic = Result<Value>(*)(Interpreter &i, std::vector<Value>);

/// Lazy Array / Continuation / Closure type thingy
struct Block
{
	/// Location of definition / creation
	Location location;

	/// Names of expected parameters
	std::vector<std::string> parameters;

	/// Body that will be executed
	Ast body;

	/// Context from which block was created. Used for closures
	std::shared_ptr<Env> context;

	/// Calling block
	Result<Value> operator()(Interpreter &i, std::vector<Value> params);

	/// Indexing block
	Result<Value> index(Interpreter &i, unsigned position) const;

	/// Count of elements in block
	usize size() const;
};

#endif
