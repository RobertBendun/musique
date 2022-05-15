#include <musique.hh>
#include <iostream>

static Ast wrap_if_several(std::vector<Ast> &&ast, Ast(*wrapper)(std::vector<Ast>));
static Result<std::vector<Ast>> parse_one_or_more(Parser &p, Result<Ast> (Parser::*parser)(), std::optional<Token::Type> separator = std::nullopt);

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

	auto const result = parser.parse_sequence();

	if (parser.token_id < parser.tokens.size()) {
		errors::all_tokens_were_not_parsed(std::span(parser.tokens).subspan(parser.token_id));
	}

	return result;
}

Result<Ast> Parser::parse_sequence()
{
	auto seq = Try(parse_one_or_more(*this, &Parser::parse_expression, Token::Type::Expression_Separator));
	return wrap_if_several(std::move(seq), Ast::sequence);
}

Result<Ast> Parser::parse_expression()
{
	return parse_infix_expression();
}

Result<Ast> Parser::parse_infix_expression()
{
	auto atomics = Try(parse_one_or_more(*this, &Parser::parse_atomic_expression));
	auto lhs = wrap_if_several(std::move(atomics), Ast::call);

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

	case Token::Type::Open_Block:
		{
			auto opening = consume();
			if (expect(Token::Type::Close_Block)) {
				consume();
				return Ast::block(std::move(opening).location);
			}

			auto start = token_id;
			std::vector<Ast> parameters;

			if (auto p = parse_one_or_more(*this, &Parser::parse_identifier); p && expect(Token::Type::Variable_Separator)) {
				consume();
				parameters = std::move(p).value();
			} else {
				token_id = start;
			}

			return parse_sequence().and_then([&](Ast ast) -> tl::expected<Ast, Error> {
				Try(ensure(Token::Type::Close_Block));
				consume();
				return Ast::block(opening.location, ast, std::move(parameters));
			});
		}

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

Result<Ast> Parser::parse_identifier()
{
	Try(ensure(Token::Type::Symbol));
	auto lit = Ast::literal(consume());
	while (expect(Token::Type::Expression_Separator)) { consume(); }
	return lit;
}

Result<std::vector<Ast>> parse_one_or_more(Parser &p, Result<Ast> (Parser::*parser)(), std::optional<Token::Type> separator)
{
	std::vector<Ast> trees;
	Result<Ast> expr;
	while ((expr = (p.*parser)()).has_value()) {
		trees.push_back(std::move(expr).value());
		if (separator) {
			if (auto s = p.ensure(*separator); !s.has_value()) {
				break;
			}
			do p.consume(); while (p.expect(*separator));
		}
	}

	if (trees.empty()) {
		return expr.error();
	}

	return trees;
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
	ast.location = token.location;
	ast.token = std::move(token);
	return ast;
}

Ast Ast::binary(Token token, Ast lhs, Ast rhs)
{
	Ast ast;
	ast.type = Type::Binary;
	ast.location = token.location;
	ast.token = std::move(token);
	ast.arguments.push_back(std::move(lhs));
	ast.arguments.push_back(std::move(rhs));
	return ast;
}

Ast Ast::call(std::vector<Ast> call)
{
	assert(!call.empty(), "Call must have at least pice of code that is beeing called");

	Ast ast;
	ast.type = Type::Call;
	ast.location = call.front().location;
	ast.arguments = std::move(call);
	return ast;
}

Ast Ast::sequence(std::vector<Ast> expressions)
{
	Ast ast;
	ast.type = Type::Sequence;
	if (!expressions.empty()) {
		ast.location = expressions.front().location;
	}
	ast.arguments = std::move(expressions);
	return ast;
}

Ast Ast::block(Location location, Ast seq, std::vector<Ast> parameters)
{
	Ast ast;
	ast.type = Type::Block;
	ast.location = location;
	ast.arguments = std::move(parameters);
	ast.arguments.push_back(std::move(seq));
	return ast;
}

Ast wrap_if_several(std::vector<Ast> &&ast, Ast(*wrapper)(std::vector<Ast>))
{
	if (ast.size() == 1)
		return std::move(ast)[0];
	return wrapper(std::move(ast));
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

	case Ast::Type::Block:
	case Ast::Type::Call:
	case Ast::Type::Sequence:
		return lhs.arguments.size() == rhs.arguments.size()
			&& std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin());
	}

	unreachable();
}

std::ostream& operator<<(std::ostream& os, Ast::Type type)
{
	switch (type) {
	case Ast::Type::Binary:   return os << "BINARY";
	case Ast::Type::Block:    return os << "BLOCK";
	case Ast::Type::Call:     return os << "CALL";
	case Ast::Type::Literal:  return os << "LITERAL";
	case Ast::Type::Sequence: return os << "SEQUENCE";
	}
	unreachable();
}

std::ostream& operator<<(std::ostream& os, Ast const& tree)
{
	os << "Ast::" << tree.type << "(" << tree.token.source << ")";
	if (!tree.arguments.empty()) {
		os << " { ";
		for (auto const& arg : tree.arguments) {
			os << arg << ' ';
		}
		os << '}';
	}

	return os;
}

struct Indent
{
	unsigned count;
	Indent operator+() { return { count + 2 }; }
	operator unsigned() { return count; }
};

std::ostream& operator<<(std::ostream& os, Indent n)
{
	while (n.count-- > 0)
		os.put(' ');
	return os;
}

void dump(Ast const& tree, unsigned indent)
{
	Indent i{indent};
	std::cout << i << "Ast::" << tree.type << "(" << tree.token.source << ")";
	if (!tree.arguments.empty()) {
		std::cout << " {\n";
		for (auto const& arg : tree.arguments) {
			dump(arg, +i);
		}
		std::cout << i << '}';
	}
	std::cout << '\n';
}
