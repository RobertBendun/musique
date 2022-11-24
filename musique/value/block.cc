#include <musique/interpreter/env.hh>
#include <musique/interpreter/interpreter.hh>
#include <musique/try.hh>
#include <musique/value/block.hh>
#include <musique/value/value.hh>

inline bool is_ast_collection(Ast::Type type)
{
	return type == Ast::Type::Sequence || type == Ast::Type::Concurrent;
}

/// Helper that produces error when trying to access container with too few elements for given index
static inline std::optional<Error> guard_index(unsigned index, unsigned size)
{
	if (index < size) return {};
	return Error {
		.details = errors::Out_Of_Range { .required_index = index, .size = size }
	};
}

// TODO Add memoization
Result<Value> Block::index(Interpreter &i, unsigned position) const
{
	ensure(parameters.empty(), "cannot index into block with parameters (for now)");
	if (is_ast_collection(body.type)) {
		Try(guard_index(position, body.arguments.size()));
		return i.eval((Ast)body.arguments[position]);
	}

	Try(guard_index(position, 1));
	return i.eval((Ast)body);
}

usize Block::size() const
{
	return is_ast_collection(body.type) ? body.arguments.size() : 1;
}

Result<Value> Block::operator()(Interpreter &i, std::vector<Value> arguments) const
{
	auto old_scope = std::exchange(i.env, context);
	i.enter_scope();

	if (parameters.size() > arguments.size()) {
		return errors::Wrong_Arity_Of {
			.type = errors::Wrong_Arity_Of::Function,
			 // TODO Let user defined functions have name of their first assigment (Zig like)
			 //      or from place of definition like <block at file:line:column>
			.name = "<block>",
			.expected_arity = parameters.size(),
			.actual_arity = arguments.size(),
		};
	}

	for (usize j = 0; j < std::min(parameters.size(), arguments.size()); ++j) {
		i.env->force_define(parameters[j], std::move(arguments[j]));
	}

	Ast body_copy = body;
	auto result = i.eval(std::move(body_copy));

	i.env = old_scope;
	return result;
}

bool Block::is_collection() const
{
	return parameters.empty();
}
