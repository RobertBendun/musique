#include <musique/parser/parser.hh>
#include <musique/lexer/lexer.hh>
#include <musique/try.hh>

#include <iostream>
#include <numeric>

static std::optional<usize> precedense(std::string_view op);

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

Result<Ast> Parser::parse(std::string_view source, std::string_view filename, unsigned line_number, bool print_tokens)
{
	// TODO Fix line number calculation
	(void)line_number;

	Lexer lexer{source};
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

#if 0
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
#endif

	return result;
}

Result<Ast> Parser::parse_sequence()
{
	std::vector<Ast> seq;
	for (;;) {
		while (expect(Token::Type::Nl) || expect(Token::Type::Comma)) {
			consume();
		}
		if (token_id < tokens.size()) {
			seq.push_back(Try(parse_expression()));
		} else {
			break;
		}
	}
	return Ast::sequence(std::move(seq));
}

Result<Ast> Parser::parse_expression()
{
	if (expect(Token::Type::Symbol, std::pair{Token::Type::Operator, "="sv})) {
		return parse_assigment();
	}
	return parse_infix();
}

Result<Ast> Parser::parse_infix()
{
	auto lhs = Try(parse_arithmetic_prefix());

	bool const next_is_operator = expect(Token::Type::Operator)
		|| expect(std::pair{Token::Type::Keyword, "and"sv})
		|| expect(std::pair{Token::Type::Keyword, "or"sv});

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

// TODO Move from
// >   *        <
// >   | \      <
// >   1  *     <
// >     / \    <
// >    2   3   <
// to
// >     *      <
// >    /|\     <
// >   1 2 3    <
Result<Ast> Parser::parse_rhs_of_infix(Ast &&lhs)
{
	auto rhs = Try(parse_arithmetic_prefix());

	bool const next_is_operator = expect(Token::Type::Operator)
		|| expect(std::pair{Token::Type::Keyword, "and"sv})
		|| expect(std::pair{Token::Type::Keyword, "or"sv});

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
	if (expect(std::pair{Token::Type::Operator, "-"sv}) || expect(std::pair{Token::Type::Operator, "+"sv})) {
		// TODO Add unary operator AST node type
		Ast unary;
		unary.token = consume();
		unary.file = unary.token.location(filename);
		unary.arguments.push_back(Try(parse_index_or_function_call()));
		return unary;
	}

	return parse_index_or_function_call();
}

Result<Ast> Parser::parse_index_or_function_call()
{
	auto result = Try(parse_atomic());
	while (token_id < tokens.size()) {
		auto const& token = tokens[token_id];
		switch (token.type) {
		break; case Token::Type::Bra:        result = Try(parse_function_call(std::move(result)));
		break; case Token::Type::Open_Index: result = Try(parse_index(std::move(result)));
		break; default: return result;
		}
	}
	return result;
}

Result<Ast> Parser::parse_function_call(Ast &&callee)
{
	auto open = consume();
	ensure(open.type == Token::Type::Bra, "parse_function_call must be called only when it can parse function call");

	Ast call;
	call.type = Ast::Type::Call;
	call.file = callee.file + open.location(filename);
	call.arguments.push_back(std::move(callee));

	for (;;) {
		ensure(token_id < tokens.size(), "Missing closing )"); // TODO(assert)

		switch (tokens[token_id].type) {
		break; case Token::Type::Ket:
			call.file += consume().location(filename);
			goto endloop;

		// FIXME We implicitly require to be comma here, this may strike us later
		break; case Token::Type::Comma: case Token::Type::Nl:
			while (expect(Token::Type::Comma) || expect(Token::Type::Nl)) {
				call.file += consume().location(filename);
			}

		break; default:
			call.arguments.push_back(Try(parse_expression()));
		}
	}
endloop:

	return call;
}

Result<Ast> Parser::parse_index(Ast &&indexable)
{
	auto open = consume();
	ensure(open.type == Token::Type::Open_Index, "parse_function_call must be called only when it can parse function call");

	Ast index;
	index.type = Ast::Type::Binary;
	index.token = open;
	index.file = indexable.file + open.location(filename);
	index.arguments.push_back(std::move(indexable));

	// This indirection is killing me, fixme pls
	auto &block = index.arguments.emplace_back();
	block.type = Ast::Type::Block;

	auto &sequence = block.arguments.emplace_back();
	sequence.type = Ast::Type::Sequence;
	sequence.file = index.file;

	for (;;) {
		ensure(token_id < tokens.size(), "Missing closing ]"); // TODO(assert)

		switch (tokens[token_id].type) {
		break; case Token::Type::Close_Index:
			sequence.file += consume().location(filename);
			index.file += sequence.file;
			goto endloop;

		// FIXME We implicitly require to be comma here, this may strike us later
		break; case Token::Type::Comma: case Token::Type::Nl:
			while (expect(Token::Type::Comma) || expect(Token::Type::Nl)) {
				sequence.file += consume().location(filename);
			}

		break; default:
			sequence.arguments.push_back(Try(parse_expression()));
		}
	}
endloop:

	index.file += sequence.file;
	return index;
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

	ensure(expect(std::pair{Token::Type::Operator, "="sv}), "This function should always be called with valid sequence");
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

Result<Ast> Parser::parse_note()
{
	auto note = Ast::literal(filename, consume());
	std::optional<Ast> arg;

	bool const followed_by_fraction = token_id+1 < tokens.size()
		&& one_of(tokens[token_id+0].type, Token::Type::Symbol, Token::Type::Numeric)
		&&        tokens[token_id+1].type == Token::Type::Operator && tokens[token_id+1].source == "/"
		&& one_of(tokens[token_id+2].type, Token::Type::Symbol, Token::Type::Numeric);

	bool const followed_by_scalar = token_id < tokens.size()
		&& one_of(tokens[token_id].type, Token::Type::Symbol, Token::Type::Numeric);

	if (!(followed_by_fraction || followed_by_scalar)) {
		return note;
	}

	Ast call;
	call.type = Ast::Type::Call;
	call.file = note.file;
	call.arguments.push_back(std::move(note));

	if (followed_by_fraction) {
		auto lhs = Ast::literal(filename, consume());

		auto op = consume();
		ensure(op.type == Token::Type::Operator && op.source == "/", "Condition that allows entry to this block must be wrong");

		auto rhs = Ast::literal(filename, consume());

		call.arguments.push_back(Ast::binary(filename, std::move(op), std::move(lhs), std::move(rhs)));
	} else if (followed_by_scalar) {
		call.arguments.push_back(Ast::literal(filename, consume()));
	} else {
		unreachable();
	}

	return call;
}

// Block parsing algorithm has awful performance, branch predictions and all this stuff
// The real culprit is a syntax probably, but this function needs to be investigated
Result<Ast> Parser::parse_block()
{
	auto open = consume();
	ensure(open.type == Token::Type::Bra, "parse_block expects that it can parse");

	if (expect(Token::Type::Ket)) {
		return Ast::block(open.location(filename) + consume().location(filename), Ast::sequence({}));
	}

	Ast sequence;
	sequence.type = Ast::Type::Sequence;
	sequence.file = open.location(filename);

	auto start_token_offset = token_id;

	bool is_lambda = false;
	std::vector<Ast> parameters;

	while (token_id < tokens.size()) {
		auto const& current_token = tokens[token_id];
		switch (current_token.type) {
		break; case Token::Type::Ket:
			sequence.file += consume().location(filename);
			goto endloop;

		break; case Token::Type::Parameter_Separator:
		{
			ensure(not is_lambda, "Only one parameter separator is allowed inside a block"); // TODO(assert)
			std::vector<unsigned> identifier_looking;

			for (auto i = start_token_offset; i < token_id; ++i) {
				switch (tokens[i].type) {
				case Token::Type::Chord:
				case Token::Type::Keyword:
					identifier_looking.push_back(i);
					break;

				case Token::Type::Symbol:
					continue;

				// TODO Weird things before parameter separator error
				default:
					unimplemented();
				}
			}

			// TODO Report all instances of identifier looking parameters
			if (identifier_looking.size()) {
				auto const& token = tokens[identifier_looking.front()];
				return Error {
					.details = errors::Literal_As_Identifier {
						.type_name = std::string(type_name(token.type)),
						.source    = std::string(token.source),
						.context   = "block parameter list"
					},
				};
			}

			consume();
			is_lambda = true;
			std::swap(sequence.arguments, parameters);
		}

		break; case Token::Type::Comma: case Token::Type::Nl:
			sequence.file += consume().location(filename);

		break; default:
			sequence.arguments.push_back(Try(parse_expression()));
		}
	}
endloop:

	if (is_lambda) {
		return Ast::lambda(sequence.file, std::move(sequence), std::move(parameters));
	}

	// TODO This double nesting is unnesesary, remove it
	Ast block;
	block.type = Ast::Type::Block;
	block.file = sequence.file;
	block.arguments.push_back(std::move(sequence));
	return block;
}

Result<Ast> Parser::parse_atomic()
{
	auto token = Try(peek());
	switch (token.type) {
	case Token::Type::Keyword:
		// Not all keywords are literals. Keywords like `true` can be part of atomic expression (essentialy single value like)
		// but keywords like `var` announce variable declaration which is higher up in expression parsing.
		// So we need to explicitly allow only keywords that are also literals
		if (std::find(Literal_Keywords.begin(), Literal_Keywords.end(), token.source) == Literal_Keywords.end()) {
			return Error {
				.details = errors::Unexpected_Keyword { .keyword = std::string(peek()->source) },
				.file = token.location(filename)
			};
		}
		[[fallthrough]];
	case Token::Type::Numeric:
	case Token::Type::Symbol:
		return Ast::literal(filename, consume());

	break; case Token::Type::Chord:
		return parse_note();

	break; case Token::Type::Bra:
		return parse_block();

	break; default:
		std::cout << "Token at position: " << token_id << " with value: " << *peek() << std::endl;
		unimplemented();
	}
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

Token Parser::consume([[maybe_unused]] Location const& location)
{
	ensure(token_id < tokens.size(), "Cannot consume token when no tokens are available");
	// std::cout << "-- Token " << tokens[token_id] << " consumed at " << location << std::endl;
	return std::move(tokens[token_id++]);
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

	case Ast::Type::Unary:
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
	case Ast::Type::Unary:                return os << "UNARY";
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
