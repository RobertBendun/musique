#include <musique.hh>

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
	return parse_binary_operator();
}

Result<Ast> Parser::parse_binary_operator()
{
	return parse_literal();
}

Result<Ast> Parser::parse_literal()
{
	Try(ensure(Token::Type::Numeric));
	return Ast::literal(consume());
}

Token Parser::consume()
{
	return std::move(tokens[token_id++]);
}

bool Parser::expect(Token::Type type) const
{
	return token_id < tokens.size() && tokens[token_id].type == type;
}

Result<Unit> Parser::ensure(Token::Type type) const
{
	return token_id >= tokens.size()
		? errors::unexpected_end_of_source(tokens.back().location)
		: tokens[token_id].type != type
		? errors::unexpected_token(type, tokens[token_id])
		: Result<Unit>{};
}

Ast Ast::literal(Token token)
{
	Ast ast;
	ast.type = Ast::Type::Literal;
	ast.token = std::move(token);
	return ast;
}
