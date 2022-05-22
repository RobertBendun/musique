#include <musique.hh>
#include <iostream>

static Ast wrap_if_several(std::vector<Ast> &&ast, Ast(*wrapper)(std::vector<Ast>));

constexpr auto Literal_Keywords = std::array {
	"false"sv,
	"nil"sv,
	"true"sv,
};

enum class At_Least : bool
{
	Zero,
	One
};

static Result<std::vector<Ast>> parse_many(
	Parser &p,
	Result<Ast> (Parser::*parser)(),
	std::optional<Token::Type> separator,
	At_Least at_least);

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
	auto seq = Try(parse_many(*this, &Parser::parse_expression, Token::Type::Expression_Separator, At_Least::Zero));
	return wrap_if_several(std::move(seq), Ast::sequence);
}

Result<Ast> Parser::parse_expression()
{
	auto var = parse_variable_declaration();
	if (!var.has_value())
		return parse_infix_expression();
	return var;
}

Result<Ast> Parser::parse_variable_declaration()
{
	if (!expect(Token::Type::Keyword, "var")) {
		return errors::expected_keyword(Try(peek()), "var");
	}
	auto var = consume();

	auto lvalue = Try(parse_many(*this, &Parser::parse_identifier, std::nullopt, At_Least::One));

	if (expect(Token::Type::Operator, "=")) {
		consume();
		return Ast::variable_declaration(var.location, std::move(lvalue), Try(parse_expression()));
	}

	return Ast::variable_declaration(var.location, std::move(lvalue), std::nullopt);
}

Result<Ast> Parser::parse_infix_expression()
{
	auto atomics = Try(parse_many(*this, &Parser::parse_atomic_expression, std::nullopt, At_Least::One));
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
	case Token::Type::Keyword:
		if (std::find(Literal_Keywords.begin(), Literal_Keywords.end(), peek()->source) == Literal_Keywords.end()) {
			return errors::unexpected_token(*peek());
		}
		[[fallthrough]];
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
			bool is_lambda = false;

			if (expect(Token::Type::Parameter_Separator)) {
				consume();
				is_lambda = true;
			} else {
				auto p = parse_many(*this, &Parser::parse_identifier_with_trailing_separators, std::nullopt, At_Least::One);
				if (p && expect(Token::Type::Parameter_Separator)) {
					consume();
					parameters = std::move(p).value();
					is_lambda = true;
				} else {
					token_id = start;
				}
			}

			return parse_sequence().and_then([&](Ast &&ast) -> Result<Ast> {
				Try(ensure(Token::Type::Close_Block));
				consume();
				if (is_lambda) {
					return Ast::lambda(opening.location, std::move(ast), std::move(parameters));
				} else {
					return Ast::block(opening.location, std::move(ast));
				}
			});
		}

	case Token::Type::Open_Paren:
		consume();
		return parse_expression().and_then([&](Ast ast) -> Result<Ast> {
			Try(ensure(Token::Type::Close_Paren));
			consume();
			return ast;
		});

	default:
		return peek().and_then([](auto const& token) -> Result<Ast> {
			return tl::unexpected(errors::unexpected_token(token));
		});
	}
}

Result<Ast> Parser::parse_identifier_with_trailing_separators()
{
	Try(ensure(Token::Type::Symbol));
	auto lit = Ast::literal(consume());
	while (expect(Token::Type::Expression_Separator)) { consume(); }
	return lit;
}

Result<Ast> Parser::parse_identifier()
{
	Try(ensure(Token::Type::Symbol));
	return Ast::literal(consume());
}

static Result<std::vector<Ast>> parse_many(
	Parser &p,
	Result<Ast> (Parser::*parser)(),
	std::optional<Token::Type> separator,
	At_Least at_least)
{
	std::vector<Ast> trees;
	Result<Ast> expr;

	if (at_least == At_Least::Zero && p.token_id >= p.tokens.size())
		return {};

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

bool Parser::expect(Token::Type type, std::string_view lexeme) const
{
	return token_id < tokens.size() && tokens[token_id].type == type && tokens[token_id].source == lexeme;
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
		ast.arguments = std::move(expressions);
	}
	return ast;
}

Ast Ast::block(Location location, Ast seq)
{
	Ast ast;
	ast.type = Type::Block;
	ast.location = location;
	ast.arguments.push_back(std::move(seq));
	return ast;
}

Ast Ast::lambda(Location location, Ast body, std::vector<Ast> parameters)
{
	Ast ast;
	ast.type = Type::Lambda;
	ast.location = location;
	ast.arguments = std::move(parameters);
	ast.arguments.push_back(std::move(body));
	return ast;
}

Ast Ast::variable_declaration(Location loc, std::vector<Ast> lvalues, std::optional<Ast> rvalue)
{
	Ast ast;
	ast.type = Type::Variable_Declaration;
	ast.location = loc;
	ast.arguments = std::move(lvalues);
	if (rvalue) {
		ast.arguments.push_back(*std::move(rvalue));
	}
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
	case Ast::Type::Lambda:
	case Ast::Type::Sequence:
	case Ast::Type::Variable_Declaration:
		return lhs.arguments.size() == rhs.arguments.size()
			&& std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin());
	}

	unreachable();
}

std::ostream& operator<<(std::ostream& os, Ast::Type type)
{
	switch (type) {
	case Ast::Type::Binary:               return os << "BINARY";
	case Ast::Type::Block:                return os << "BLOCK";
	case Ast::Type::Call:                 return os << "CALL";
	case Ast::Type::Lambda:               return os << "LAMBDA";
	case Ast::Type::Literal:              return os << "LITERAL";
	case Ast::Type::Sequence:             return os << "SEQUENCE";
	case Ast::Type::Variable_Declaration: return os << "VAR";
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
	std::fill_n(std::ostreambuf_iterator<char>(os), n.count, ' ');
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
