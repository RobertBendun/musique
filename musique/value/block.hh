#ifndef MUSIQUE_BLOCK_HH
#define MUSIQUE_BLOCK_HH

#include <memory>
#include <musique/parser/ast.hh>
#include <musique/result.hh>
#include <musique/value/collection.hh>
#include <musique/value/function.hh>

struct Env;
struct Interpreter;
struct Value;

/// Lazy Array / Continuation / Closure type thingy
struct Block : Collection, Function
{
	~Block() override = default;

	/// Location of definition / creation
	Location location;

	/// Names of expected parameters
	std::vector<std::string> parameters;

	/// Body that will be executed
	Ast body;

	/// Context from which block was created. Used for closures
	std::shared_ptr<Env> context;

	/// Calling block
	Result<Value> operator()(Interpreter &i, std::vector<Value> params) const override;

	/// Indexing block
	Result<Value> index(Interpreter &i, unsigned position) const override;

	/// Count of elements in block
	usize size() const override;

	bool is_collection() const override;
};

#endif
