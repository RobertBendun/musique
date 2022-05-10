#include <musique.hh>
#include <iostream>

Result<Ast> Parser::parse(std::string_view source, std::string_view filename)
{
	Lexer lexer{source};
	lexer.location.filename = filename;
	Parser parser;

	for (;;) if (auto maybe_token = lexer.next_token(); maybe_token.has_value()) {
		parser.tokens.emplace_back(*std::move(maybe_token));
	} else if (maybe_token.error().type == errors::End_Of_File) {
		break;
	} else {
		return std::move(maybe_token).error();
	}

	auto const result = parser.parse_expression();

	if (parser.token_id < parser.tokens.size()) {
		errors::all_tokens_were_not_parsed(std::span(parser.tokens).subspan(parser.token_id));
	}

	return result;
}

Result<Ast> Parser::parse_expression()
{
	return parse_infix_expression();
}

Result<Ast> Parser::parse_infix_expression()
{
	std::vector<Ast> atomics;
	Result<Ast> expr;
	while ((expr = parse_atomic_expression()).has_value()) {
		atomics.push_back(std::move(expr).value());
	}

	if (atomics.empty()) {
		return expr;
	}

	auto lhs = atomics.size() == 1
		? std::move(atomics[0])
		: Ast::call(std::move(atomics));

	if (expect(Token::Type::Operator)) {
		auto op = consume();
		return parse_expression().map([&](Ast rhs) {
			return Ast::binary(std::move(op), std::move(lhs), std::move(rhs));
		});
	}

	return lhs;
}

Result<Ast> Parser::parse_atomic_expression()
{
	switch (Try(peek_type())) {
	case Token::Type::Symbol:
	case Token::Type::Numeric:
		return Ast::literal(consume());

	case Token::Type::Open_Paren:
		consume();
		return parse_expression().and_then([&](Ast ast) -> tl::expected<Ast, Error> {
			Try(ensure(Token::Type::Close_Paren));
			consume();
			return ast;
		});

	default:
		return peek().and_then([](auto const& token) -> tl::expected<Ast, Error> {
			return tl::unexpected(errors::unexpected_token(token));
		});
	}
}

Result<Token> Parser::peek() const
{
	return token_id >= tokens.size()
		? errors::unexpected_end_of_source(tokens.back().location)
		: Result<Token>(tokens[token_id]);
}

Result<Token::Type> Parser::peek_type() const
{
	return peek().map([](Token const& token) { return token.type; });
}

Token Parser::consume()
{
	return std::move(tokens[token_id++]);
}

bool Parser::expect(Token::Type type) const
{
	return token_id < tokens.size() && tokens[token_id].type == type;
}

Result<void> Parser::ensure(Token::Type type) const
{
	return token_id >= tokens.size()
		? errors::unexpected_end_of_source(tokens.back().location)
		: tokens[token_id].type != type
		? errors::unexpected_token(type, tokens[token_id])
		: Result<void>{};
}

Ast Ast::literal(Token token)
{
	Ast ast;
	ast.type = Type::Literal;
	ast.token = std::move(token);
	return ast;
}

Ast Ast::binary(Token token, Ast lhs, Ast rhs)
{
	Ast ast;
	ast.type = Type::Binary;
	ast.token = std::move(token);
	ast.arguments.push_back(std::move(lhs));
	ast.arguments.push_back(std::move(rhs));
	return ast;
}

Ast Ast::call(std::vector<Ast> call)
{
	Ast ast;
	ast.type = Type::Call;
	ast.arguments = std::move(call);
	return ast;
}

bool operator==(Ast const& lhs, Ast const& rhs)
{
	if (lhs.type != rhs.type) {
		return false;
	}

	switch (lhs.type) {
	case Ast::Type::Literal:
		return lhs.token.type == rhs.token.type && lhs.token.source == rhs.token.source;

	case Ast::Type::Binary:
		return lhs.token.type == rhs.token.type
			&& lhs.token.source == rhs.token.source
			&& lhs.arguments.size() == rhs.arguments.size()
			&& std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin());

	case Ast::Type::Call:
		return lhs.arguments.size() == rhs.arguments.size()
			&& std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin());
	}

	unreachable();
}

std::ostream& operator<<(std::ostream& os, Ast const& tree)
{
	os << "Ast(" << tree.token.source << ")";
	if (!tree.arguments.empty()) {
		os << " { ";
		for (auto const& arg : tree.arguments) {
			os << arg << ' ';
		}
		os << '}';
	}
	return os;
}
