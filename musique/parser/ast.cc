#include <musique/errors.hh>
#include <musique/parser/ast.hh>

// Don't know if it's a good idea to defer parsing of literal values up to value creation, which is current approach.
// This may create unexpected performance degradation during program evaluation.
Ast Ast::literal(std::string_view filepath, Token token)
{
	Ast ast;
	ast.type = Type::Literal;
	ast.file = token.location(filepath);
	ast.token = std::move(token);
	return ast;
}

Ast Ast::binary(std::string_view filepath, Token token, Ast lhs, Ast rhs)
{
	Ast ast;
	ast.type = Type::Binary;
	ast.file = token.location(filepath) + lhs.file + rhs.file;
	ast.token = std::move(token);
	ast.arguments.push_back(std::move(lhs));
	ast.arguments.push_back(std::move(rhs));
	return ast;
}

Ast Ast::call(std::vector<Ast> call)
{
	ensure(!call.empty(), "Call must have at least pice of code that is beeing called");

	Ast ast;
	ast.type = Type::Call;
	// TODO Accumulate file information
	ast.file = call.front().file;
	for (auto const& node : std::span(call).subspan(1))
		ast.file += node.file;
	ast.arguments = std::move(call);
	return ast;
}

Ast Ast::sequence(std::vector<Ast> expressions)
{
	Ast ast;
	ast.type = Type::Sequence;
	if (!expressions.empty()) {
		ast.file = expressions.front().file;
		for (auto const& node : std::span(expressions).subspan(1)) {
			ast.file += node.file;
		}
		ast.arguments = std::move(expressions);
	}
	return ast;
}

Ast Ast::block(File_Range file, Ast seq)
{
	Ast ast;
	ast.type = Type::Block;
	ast.file = file;
	ast.arguments.push_back(std::move(seq));
	return ast;
}

Ast Ast::lambda(File_Range file, Ast body, std::vector<Ast> parameters)
{
	Ast ast;
	ast.type = Type::Lambda;
	ast.file = file;
	ast.arguments = std::move(parameters);
	ast.arguments.push_back(std::move(body));
	return ast;
}

Ast Ast::variable_declaration(File_Range file, std::vector<Ast> lvalues, std::optional<Ast> rvalue)
{
	Ast ast;
	ast.type = Type::Variable_Declaration;
	ast.file = file;
	ast.arguments = std::move(lvalues);
	if (rvalue) {
		ast.arguments.push_back(*std::move(rvalue));
	}
	return ast;
}


