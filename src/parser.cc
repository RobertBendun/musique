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

	for (;;) {
		auto token_or_eof = Try(lexer.next_token());
		if (auto maybe_token = std::get_if<Token>(&token_or_eof); maybe_token) {
			parser.tokens.emplace_back(std::move(*maybe_token));
		} else {
			// We encountered end of file so no more tokens, break the loop
			break;
		}
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
		Error error;
		errors::Expected_Keyword kw { .keyword = "var" };
		if (token_id >= tokens.size()) {
			kw.received_type = ""; // TODO Token type
			error.location = peek()->location;
		}
		error.details = std::move(kw);
		return error;
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
		// Not all keywords are literals. Keywords like `true` can be part of atomic expression (essentialy single value like)
		// but keywords like `var` announce variable declaration which is higher up in expression parsing.
		// So we need to explicitly allow only keywords that are also literals
		if (std::find(Literal_Keywords.begin(), Literal_Keywords.end(), peek()->source) == Literal_Keywords.end()) {
			return Error {
				.details = errors::Unexpected_Keyword { .keyword = peek()->source },
				.location = peek()->location
			};
		}
		[[fallthrough]];
	case Token::Type::Chord:
	case Token::Type::Numeric:
	case Token::Type::Symbol:
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
				if (not expect(Token::Type::Close_Block)) {
					unimplemented("Error handling of this code is not implemented yet");
				}
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
			if (not expect(Token::Type::Close_Paren)) {
				unimplemented("Error handling of this code is not implemented yet");
			}
			consume();
			return ast;
		});

	default:
		return Error {
			.details = errors::internal::Unexpected_Token {
				.type = "", // TODO fill type
				.source = peek()->source,
				.when = "atomic expression parsing"
			},
			.location = peek()->location
		};
	}
}

Result<Ast> Parser::parse_identifier_with_trailing_separators()
{
	if (not expect(Token::Type::Symbol)) {
		// TODO Specific error message
		return Error {
			.details = errors::internal::Unexpected_Token {
				.type = "", // TODO fill type
				.source = peek()->source,
				.when = "identifier parsing"
			},
			.location = peek()->location
		};
	}
	auto lit = Ast::literal(consume());
	while (expect(Token::Type::Expression_Separator)) { consume(); }
	return lit;
}

Result<Ast> Parser::parse_identifier()
{
	if (not expect(Token::Type::Symbol)) {
		// TODO Specific error message
		return Error {
			.details = errors::internal::Unexpected_Token {
				.type = "", // TODO fill type
				.source = peek()->source,
				.when = "identifier parsing"
			},
			.location = peek()->location
		};
	}
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
			if (not p.expect(*separator)) {
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
		? Error {
				.details = errors::Unexpected_Empty_Source {},
				.location = tokens.back().location
			}
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

// Don't know if it's a good idea to defer parsing of literal values up to value creation, which is current approach.
// This may create unexpected performance degradation during program evaluation.
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
	if (indent == 0) {
		std::cout << std::flush;
	}
}
