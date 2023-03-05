#include <musique/parser/parser.hh>
#include <musique/lexer/lexer.hh>
#include <musique/try.hh>

#include <iostream>
#include <numeric>

static std::optional<usize> precedense(std::string_view op);

static inline bool is_identifier_looking(Token::Type type)
{
	switch (type) {
	case Token::Type::Symbol:
	case Token::Type::Keyword:
	case Token::Type::Chord:
		return true;
	default:
		return false;
	}
}

constexpr auto Literal_Keywords = std::array {
	"false"sv,
	"nil"sv,
	"true"sv,
};

static_assert(Keywords_Count == Literal_Keywords.size() + 2, "Ensure that all literal keywords are listed");

constexpr auto Operator_Keywords = std::array {
	"and"sv,
	"or"sv
};

static_assert(Keywords_Count == Operator_Keywords.size() + 3, "Ensure that all keywords that are operators are listed here");

enum class At_Least : bool
{
	Zero,
	One
};

static Result<std::vector<Ast>> parse_many(
	Parser &p,
	Result<Ast> (Parser::*parser)(),
	At_Least,
	bool(*separator)(Parser &));

Result<Ast> Parser::parse(std::string_view source, std::string_view filename, unsigned line_number, bool print_tokens)
{
	// TODO Fix line number calculation
	(void)line_number;

	// TODO Fix filename marking on ast nodes
	(void)filename;

	Lexer lexer{source};
	lexer.location = line_number;
	Parser parser;

	parser.filename = filename;

	for (;;) {
		auto token_or_eof = Try(lexer.next_token());
		if (auto maybe_token = std::get_if<Token>(&token_or_eof); maybe_token) {
			if (print_tokens) {
				std::cout << *maybe_token << std::endl;
			}
			parser.tokens.emplace_back(std::move(*maybe_token));
		} else {
			// We encountered end of file so no more tokens, break the loop
			break;
		}
	}

	auto const result = parser.parse_sequence();

	if (result.has_value() && parser.token_id < parser.tokens.size()) {
		if (parser.expect(Token::Type::Ket)) {
			auto const tok = parser.consume();
			return Error {
				.details = errors::Closing_Token_Without_Opening {
						errors::Closing_Token_Without_Opening::Paren
				},
				.file = tok.location(filename),
			};
		}

		errors::all_tokens_were_not_parsed(std::span(parser.tokens).subspan(parser.token_id));
	}

	return result;
}

Result<Ast> Parser::parse_sequence()
{
	auto seq = Try(parse_many(*this, &Parser::parse_expression, At_Least::Zero, +[](Parser &p) {
		return p.expect(Token::Type::Comma) || p.expect(Token::Type::Nl);
	}));
	return Ast::sequence(std::move(seq));
}

Result<Ast> Parser::parse_expression()
{
	if (expect(Token::Type::Symbol, Token::Type::Operator, "=")) {
		return parse_assigment();
	}
	return parse_infix();
}

Result<Ast> Parser::parse_infix()
{
	auto lhs = Try(parse_arithmetic_prefix());

	bool const next_is_operator = expect(Token::Type::Operator)
		|| expect(Token::Type::Keyword, "and")
		|| expect(Token::Type::Keyword, "or");

	if (next_is_operator) {
		auto op = consume();

		Ast ast;
		ast.file = op.location(filename) + lhs.file;
		ast.type = Ast::Type::Binary;
		ast.token = std::move(op);
		ast.arguments.emplace_back(std::move(lhs));
		return parse_rhs_of_infix(std::move(ast));
	}

	return lhs;
}

Result<Ast> Parser::parse_rhs_of_infix(Ast &&lhs)
{
	auto rhs = Try(parse_arithmetic_prefix());

	bool const next_is_operator = expect(Token::Type::Operator)
		|| expect(Token::Type::Keyword, "and")
		|| expect(Token::Type::Keyword, "or");

	if (!next_is_operator) {
		lhs.arguments.emplace_back(std::move(rhs));
		return lhs;
	}

	auto op = consume();
	auto lhs_precedense = precedense(lhs.token.source);
	auto op_precedense  = precedense(op.source);

	if (!lhs_precedense) {
		return Error {
			.details = errors::Undefined_Operator { .op = std::string(lhs.token.source), },
			.file = lhs.token.location(filename)
		};
	}
	if (!op_precedense) {
		return Error {
			.details = errors::Undefined_Operator { .op = std::string(op.source), },
			.file = op.location(filename),
		};
	}

	if (*lhs_precedense >= *op_precedense) {
		lhs.arguments.emplace_back(std::move(rhs));
		Ast ast;
		ast.file = op.location(filename);
		ast.type = Ast::Type::Binary;
		ast.token = std::move(op);
		ast.arguments.emplace_back(std::move(lhs));
		return parse_rhs_of_infix(std::move(ast));
	}

	Ast ast;
	ast.file = op.location(filename);
	ast.type = Ast::Type::Binary;
	ast.token = std::move(op);
	ast.arguments.emplace_back(std::move(rhs));
	lhs.arguments.emplace_back(Try(parse_rhs_of_infix(std::move(ast))));
	return lhs;
}

Result<Ast> Parser::parse_arithmetic_prefix()
{
	if (expect(Token::Type::Operator, "-") || expect(Token::Type::Operator, "+")) {
		// TODO Add unary operator AST node type
		unimplemented();
	}

	return parse_function_call();
}

Result<Ast> parse_sequence_inside(
	Parser &parser,
	Token::Type closing_token,
	File_Range start_location,
	bool is_lambda,
	std::vector<Ast> &&parameters,
	auto &&dont_arrived_at_closing_token
)
{
	auto ast = Try(parser.parse_sequence());
	if (not parser.expect(closing_token)) {
		Try(dont_arrived_at_closing_token());
		return Error {
			.details = errors::Unexpected_Empty_Source {
				.reason = errors::Unexpected_Empty_Source::Block_Without_Closing_Bracket,
				.start = start_location
			},
			.file = parser.tokens.back().location(parser.filename)
		};
	}

	parser.consume();

	if (is_lambda) {
		return Ast::lambda(start_location, std::move(ast), std::move(parameters));
	}

	ensure(ast.type == Ast::Type::Sequence, "I dunno if this is a valid assumption tbh");
	if (ast.arguments.size() == 1) {
		return std::move(ast.arguments.front());
	}
	return Ast::block(start_location, std::move(ast));
}

Result<Ast> Parser::parse_function_call()
{
	auto result = Try(parse_index());

	while (expect(Token::Type::Bra)) {
		auto op = consume();
		auto start_location = op.location(filename);
		auto sequence = Try(parse_sequence_inside(
			*this,
			Token::Type::Ket,
			start_location,
			false,
			{},
			[]() -> std::optional<Error> { return std::nullopt; }
		));
		if (sequence.type == Ast::Type::Sequence) {
			sequence.arguments.insert(sequence.arguments.begin(), std::move(result));
			result = Ast::call(std::move(sequence.arguments));
		} else {
			result = Ast::call({ std::move(result), std::move(sequence) });
		}
	}

	return result;
}

Result<Ast> Parser::parse_index()
{
	auto result = Try(parse_atomic());

	while (expect(Token::Type::Open_Index)) {
		auto op = consume();
		auto start_location = op.location(filename);
		result = Ast::binary(
			filename,
			std::move(op),
			std::move(result),
			Try(parse_sequence_inside(
				*this,
				Token::Type::Close_Index,
				start_location,
				false,
				{},
				[]() -> std::optional<Error> { return std::nullopt; }
			))
		);
	}

	return result;
}

Result<Ast> Parser::parse_assigment()
{
	auto lvalue = parse_identifier();
	if (not lvalue.has_value()) {
		auto details = lvalue.error().details;
		if (auto ut = std::get_if<errors::internal::Unexpected_Token>(&details); ut) {
			return Error {
				.details = errors::Literal_As_Identifier {
					.type_name = ut->type,
					.source = ut->source,
					.context = "variable declaration"
				},
				.file = lvalue.error().file
			};
		}
		return std::move(lvalue).error();
	}

	ensure(expect(Token::Type::Operator, "="), "This function should always be called with valid sequence");
	consume();
	return Ast::variable_declaration(lvalue->file, { *std::move(lvalue) }, Try(parse_expression()));
}

Result<Ast> Parser::parse_identifier()
{
	if (not expect(Token::Type::Symbol)) {
		// TODO Specific error message
		return Error {
			.details = errors::internal::Unexpected_Token {
				.type = std::string(type_name(peek()->type)),
				.source = std::string(peek()->source),
				.when = "identifier parsing"
			},
			.file = peek()->location(filename)
		};
	}
	return Ast::literal(filename, consume());
}

Result<Ast> Parser::parse_atomic()
{
	switch (Try(peek_type())) {
	case Token::Type::Numeric:
	case Token::Type::Symbol:
		return Ast::literal(filename, consume());

	break; default:
		std::cout << token_id << Try(peek());
		unimplemented();
	}
}

#if 0

Result<Ast> Parser::parse_atomic_expression()
{
	switch (Try(peek_type())) {
	case Token::Type::Keyword:
		// Not all keywords are literals. Keywords like `true` can be part of atomic expression (essentialy single value like)
		// but keywords like `var` announce variable declaration which is higher up in expression parsing.
		// So we need to explicitly allow only keywords that are also literals
		if (std::find(Literal_Keywords.begin(), Literal_Keywords.end(), peek()->source) == Literal_Keywords.end()) {
			return Error {
				.details = errors::Unexpected_Keyword { .keyword = std::string(peek()->source) },
				.location = peek()->location
			};
		}
		[[fallthrough]];
	case Token::Type::Chord:
	case Token::Type::Numeric:
	case Token::Type::Symbol:
		return Ast::literal(consume());

	case Token::Type::Bra:
		{
			auto opening = consume();
			if (expect(Token::Type::Ket)) {
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

			return parse_sequence_inside(
				*this,
				Token::Type::Ket,
				opening.location,
				is_lambda,
				std::move(parameters),
				[&]() -> std::optional<Error> {
					if (!expect(Token::Type::Parameter_Separator)) {
						return std::nullopt;
					}
					if (is_lambda) {
						ensure(false, "There should be error message that you cannot put multiple parameter separators in one block");
					}

					// This may be a result of user trying to specify parameters from things that cannot be parameters (like chord literals)
					// or accidential hit of "|" on the keyboard. We can detect first case by ensuring that all tokens between this place
					// and beggining of a block are identifier looking: keywords, symbols, literal chord declarations, boolean literals etc
					std::optional<Token> invalid_token = std::nullopt;
					auto const success = std::all_of(tokens.begin() + start, tokens.begin() + (token_id - start + 1), [&](Token const& token) {
						if (!invalid_token && token.type != Token::Type::Symbol) {
							// TODO Maybe gather all tokens to provide all the help needed in the one iteration
							invalid_token = token;
						}
						return is_identifier_looking(token.type);
					});

					if (success && invalid_token) {
						return Error {
							.details = errors::Literal_As_Identifier {
								.type_name = std::string(type_name(invalid_token->type)),
								.source = std::string(invalid_token->source),
								.context = "block parameter list"
							},
							.location = invalid_token->location
						};
					}
					return std::nullopt;
				}
			);
		}

	break; case Token::Type::Operator:
		return Error {
			.details = errors::Wrong_Arity_Of {
				.type = errors::Wrong_Arity_Of::Operator,
				.name = std::string(peek()->source),
				.expected_arity = 2, // TODO This should be resolved based on operator
				.actual_arity = 0,
			},
			.location = peek()->location,
		};


	default:
		return Error {
			.details = errors::internal::Unexpected_Token {
				.type = std::string(type_name(peek()->type)),
				.source = std::string(peek()->source),
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
				.type = std::string(type_name(peek()->type)),
				.source = std::string(peek()->source),
				.when = "identifier parsing"
			},
			.location = peek()->location
		};
	}
	auto lit = Ast::literal(consume());
	while (expect(Token::Type::Comma)) { consume(); }
	return lit;
}

#endif

static Result<std::vector<Ast>> parse_many(
	Parser &p,
	Result<Ast> (Parser::*parser)(),
	At_Least at_least,
	bool (*separator)(Parser &))
{
	std::vector<Ast> trees;
	Result<Ast> expr;

	// Consume random separators laying before sequence. This was added to prevent
	// an error when input is only expression separator ";"
	while (separator && at_least == At_Least::Zero && separator(p)) {
		p.consume();
	}

	if (at_least == At_Least::Zero && p.token_id >= p.tokens.size()) {
		return {};
	}

	while ((expr = (p.*parser)()).has_value()) {
		trees.push_back(std::move(expr).value());
		if (separator) {
			if (not separator(p)) {
				break;
			}
			do p.consume(); while (separator(p));
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
				.file = tokens.back().location(filename)
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

bool Parser::expect(Token::Type t1, Token::Type t2, std::string_view lexeme_for_t2) const
{
	return token_id+1 < tokens.size()
		&& tokens[token_id].type == t1
		&& tokens[token_id+1].type == t2
		&& tokens[token_id+1].source == lexeme_for_t2;
}

Ast wrap_if_several(std::vector<Ast> &&ast, Ast(*wrapper)(std::vector<Ast>))
{
	if (ast.size() == 1)
		return std::move(ast)[0];
	return wrapper(std::move(ast));
}

constexpr bool one_of(std::string_view id, auto const& ...args)
{
	return ((id == args) || ...);
}

static std::optional<usize> precedense(std::string_view op)
{
	// Operators that are not included below are
	//  postfix index operator [] since it have own precedense rules and is not binary expression but its own kind of expression
	//  in terms of our parsing function hierarchy
	//
	//  Exclusion of them is marked by subtracting total number of excluded operators.
	static_assert(Operators_Count - 1 == 16, "Ensure that all operators have defined precedense below");

	if (one_of(op, ":=")) return 0;
	if (one_of(op, "=")) return 10;
	if (one_of(op, "or")) return 100;
	if (one_of(op, "and")) return 150;
	if (one_of(op, "<", ">", "<=", ">=", "==", "!=")) return 200;
	if (one_of(op, "+", "-")) return 300;
	if (one_of(op, "*", "/", "%", "&")) return 400;
	if (one_of(op, "**")) return 500;

	return std::nullopt;
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

std::size_t std::hash<Ast>::operator()(Ast const& value) const
{
	auto h = std::hash<Token>{}(value.token);
	h = std::accumulate(value.arguments.begin(), value.arguments.end(), h, [this](size_t h, Ast const& node) {
		return hash_combine(h, operator()(node));
	});
	return hash_combine(size_t(value.type), h);
}
