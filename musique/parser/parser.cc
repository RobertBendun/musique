#include <musique/lexer/lexer.hh>
#include <musique/parser/parser.hh>
#include <musique/try.hh>
#include <musique/location.hh>

#include <source_location>

#include <iostream>
#include <numeric>

static std::optional<usize> precedense(std::string_view op);

constexpr auto Literal_Keywords = std::array {
	"false"sv,
	"nil"sv,
	"true"sv,
};

static_assert(Keywords_Count == Literal_Keywords.size() + 8, "Ensure that all literal keywords are listed");

constexpr auto Operator_Keywords = std::array {
	"and"sv,
	"or"sv
};

static_assert(Keywords_Count == Operator_Keywords.size() + 9, "Ensure that all keywords that are operators are listed here");

enum class At_Least : bool
{
	Zero,
	One
};

static Result<Ast> parse_arithmetic_prefix(Parser &p);
static Result<Ast> parse_array_literal(Parser &p);
static Result<Ast> parse_assigment(Parser &p);
static Result<Ast> parse_atomic(Parser &p);
static Result<Ast> parse_expression(Parser &p);
static Result<Ast> parse_for(Parser &p);
static Result<Ast> parse_function_call(Parser &p, Ast &&ast);
static Result<Ast> parse_identifier(Parser &p);
static Result<Ast> parse_if_else(Parser &p);
static Result<Ast> parse_index(Parser &p, Ast &&ast);
static Result<Ast> parse_index_or_function_call(Parser &p);
static Result<Ast> parse_infix(Parser &p);
static Result<Ast> parse_note(Parser &p);
static Result<Ast> parse_paren_or_lambda(Parser &p);
static Result<Ast> parse_rhs_of_infix(Parser &p, Ast &&lhs);
static Result<Ast> parse_sequence(Parser &p);

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

	auto const result = parse_sequence(parser);

	if (result.has_value() && parser.token_id < parser.tokens.size()) {
		if (parser.expect(Token::Type::Close_Paren)) {
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

/// Skip separators (newline and comma) and return how many ware skipped
static unsigned skip_separators(Parser &parser)
{
	unsigned separators_skipped = 0;
	while (parser.expect(Token::Type::Nl) || parser.expect(Token::Type::Comma)) {
		parser.consume();
		separators_skipped++;
	}
	return separators_skipped;
}


struct log_guard {
	inline static size_t indent = 0;

	inline log_guard(Parser const& parser, std::source_location caller = std::source_location::current())
	{
		if (!std::getenv("TRACE_MUSIQUE_PARSER")) {
			return;
		}

		for (auto i = 0u; i < indent; ++i) std::cerr.put(' ');
		std::cerr << "called " << caller.function_name();
		if (parser.token_id < parser.tokens.size()) {
			std::cerr << ", next = " << parser.tokens[parser.token_id];
		}
		std::cerr << std::endl;
		indent++;
	}

	inline ~log_guard() noexcept
	{
		indent--;
	}
};

#ifdef Debug
#define log_parser_function(...) [[maybe_unused]] log_guard log_guard##__LINE__{__VA_ARGS__}
#else
#define log_parser_function(...)
#endif

// sequence = {expression, {newline|comma}}
Result<Ast> parse_sequence(Parser &p)
{
	log_parser_function(p);
	std::vector<Ast> seq;

	skip_separators(p);
	if (p.token_id < p.tokens.size()) {
		seq.push_back(Try(parse_expression(p)));
		while (skip_separators(p) >= 1) {
			bool const stop = p.token_id >= p.tokens.size()
				|| (p.tokens[p.token_id].type == Token::Type::Keyword && one_of(p.tokens[p.token_id].source, "else", "end"));
			if (stop) {
				break;
			}
			seq.push_back(Try(parse_expression(p)));
		}
	}

	return Ast::sequence(std::move(seq));
}

// expression = assigment | infix
Result<Ast> parse_expression(Parser &p)
{
	log_parser_function(p);
	if (p.expect(std::pair{Token::Type::Keyword, "if"sv})) {
		return parse_if_else(p);
	}

	if (p.expect(std::pair{Token::Type::Keyword, "for"sv})) {
		return parse_for(p);
	}

	if (p.expect(std::pair{Token::Type::Keyword, "while"sv})) {
		unimplemented();
	}

	if (p.expect(Token::Type::Symbol, std::pair{Token::Type::Operator, "="sv})) {
		return parse_assigment(p);
	}

	return parse_infix(p);
}

// if_else = 'if', expression, ('\n' | 'then'), sequence, [ 'else', sequence' ], 'end'
Result<Ast> parse_if_else(Parser &p)
{
	log_parser_function(p);
	auto if_token = p.consume();
	ensure(if_token.type == Token::Type::Keyword && if_token.source == "if", "parse_if_else() called and first token was not if");

	auto condition = Try(parse_expression(p));

	if (p.expect(std::pair{Token::Type::Keyword, "then"sv}) || p.expect(Token::Type::Nl)) {
		p.consume();
	} else {
		unimplemented("report that then or newline is required");
	}

	auto then = Try(parse_sequence(p));

	Ast if_node{};
	if_node.type = Ast::Type::If;
	if_node.token = if_token;
	if_node.arguments.push_back(std::move(condition));
	if_node.arguments.push_back(std::move(then));

	if (p.expect(std::pair{Token::Type::Keyword, "else"sv})) {
		[[maybe_unused]] auto else_token = p.consume(); // TODO Use for location resolution
		auto else_ = Try(parse_sequence(p));
		if_node.arguments.push_back(std::move(else_));
	}

	if (p.expect(std::pair{Token::Type::Keyword, "end"sv})) {
		[[maybe_unused]] auto end_token = p.consume();
		if_node.file = { .filename = p.filename, .start = if_token.start, .stop = end_token.start };
		return if_node;
	}

	unimplemented("report error: missing end in if statement");
}

/// Parse either infix expression or variable declaration
Result<Ast> parse_for(Parser &p)
{
	(void)p;
	log_parser_function(p);
	unimplemented();
}

// infix = arithmetic_prefix, [ operator, infix ]
Result<Ast> parse_infix(Parser &p)
{
	log_parser_function(p);
	auto lhs = Try(parse_arithmetic_prefix(p));

	bool const next_is_operator = p.expect(Token::Type::Operator)
		|| p.expect(std::pair{Token::Type::Keyword, "and"sv})
		|| p.expect(std::pair{Token::Type::Keyword, "or"sv});

	if (next_is_operator) {
		auto op = p.consume();

		Ast ast;
		ast.file = op.location(p.filename) + lhs.file;
		ast.type = Ast::Type::Binary;
		ast.token = std::move(op);
		ast.arguments.emplace_back(std::move(lhs));
		return parse_rhs_of_infix(p, std::move(ast));
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
Result<Ast> parse_rhs_of_infix(Parser &p, Ast &&lhs)
{
	log_parser_function(p);
	auto rhs = Try(parse_arithmetic_prefix(p));

	bool const next_is_operator = p.expect(Token::Type::Operator)
		|| p.expect(std::pair{Token::Type::Keyword, "and"sv})
		|| p.expect(std::pair{Token::Type::Keyword, "or"sv});

	if (!next_is_operator) {
		lhs.arguments.emplace_back(std::move(rhs));
		return lhs;
	}

	auto op = p.consume();
	auto lhs_precedense = precedense(lhs.token.source);
	auto op_precedense  = precedense(op.source);

	if (!lhs_precedense) {
		return Error {
			.details = errors::Undefined_Operator { .op = std::string(lhs.token.source), },
			.file = lhs.token.location(p.filename)
		};
	}
	if (!op_precedense) {
		return Error {
			.details = errors::Undefined_Operator { .op = std::string(op.source), },
			.file = op.location(p.filename),
		};
	}

	if (*lhs_precedense >= *op_precedense) {
		lhs.arguments.emplace_back(std::move(rhs));
		Ast ast;
		ast.file = op.location(p.filename);
		ast.type = Ast::Type::Binary;
		ast.token = std::move(op);
		ast.arguments.emplace_back(std::move(lhs));
		return parse_rhs_of_infix(p, std::move(ast));
	}

	Ast ast;
	ast.file = op.location(p.filename);
	ast.type = Ast::Type::Binary;
	ast.token = std::move(op);
	ast.arguments.emplace_back(std::move(rhs));
	lhs.arguments.emplace_back(Try(parse_rhs_of_infix(p, std::move(ast))));
	return lhs;
}

Result<Ast> parse_arithmetic_prefix(Parser &p)
{
	log_parser_function(p);
	if (p.expect(std::pair{Token::Type::Operator, "-"sv}) || p.expect(std::pair{Token::Type::Operator, "+"sv})) {
		Ast unary;
		unary.type = Ast::Type::Unary;
		unary.token = p.consume();
		unary.file = unary.token.location(p.filename);
		unary.arguments.push_back(Try(parse_index_or_function_call(p)));
		return unary;
	}

	return parse_index_or_function_call(p);
}

Result<Ast> parse_index_or_function_call(Parser &p)
{
	log_parser_function(p);
	auto result = Try(parse_atomic(p));
	while (p.token_id < p.tokens.size()) {
		auto const& token = p.tokens[p.token_id];
		switch (token.type) {
		break; case Token::Type::Open_Paren:        result = Try(parse_function_call(p, std::move(result)));
		break; case Token::Type::Open_Bracket: result = Try(parse_index(p, std::move(result)));
		break; default: return result;
		}
	}
	return result;
}

Result<Ast> parse_function_call(Parser &p, Ast &&callee)
{
	log_parser_function(p);
	auto open = p.consume();
	ensure(open.type == Token::Type::Open_Paren, "parse_function_call must be called only when it can parse function call");

	Ast call;
	call.type = Ast::Type::Call;
	call.file = callee.file + open.location(p.filename);
	call.arguments.push_back(std::move(callee));

	for (;;) {
		ensure(p.token_id < p.tokens.size(), "Missing closing )"); // TODO(assert)

		switch (p.tokens[p.token_id].type) {
		break; case Token::Type::Close_Paren:
			call.file += p.consume().location(p.filename);
			goto endloop;

		// FIXME We implicitly require to be comma here, this may strike us later
		break; case Token::Type::Comma: case Token::Type::Nl:
			while (p.expect(Token::Type::Comma) || p.expect(Token::Type::Nl)) {
				call.file += p.consume().location(p.filename);
			}

		break; default:
			call.arguments.push_back(Try(parse_expression(p)));
		}
	}
endloop:

	return call;
}

Result<Ast> parse_index(Parser &p, Ast &&indexable)
{
	log_parser_function(p);
	auto open = p.consume();
	ensure(open.type == Token::Type::Open_Bracket, "parse_function_call must be called only when it can parse function call");

	Ast index;
	index.type = Ast::Type::Binary;
	index.token = open;
	index.file = indexable.file + open.location(p.filename);
	index.arguments.push_back(std::move(indexable));

	auto &sequence = index.arguments.emplace_back();
	sequence.type = Ast::Type::Sequence;
	sequence.file = index.file;

	for (;;) {
		ensure(p.token_id < p.tokens.size(), "Missing closing ]"); // TODO(assert)

		switch (p.tokens[p.token_id].type) {
		break; case Token::Type::Close_Bracket:
			sequence.file += p.consume().location(p.filename);
			index.file += sequence.file;
			goto endloop;

		// FIXME We implicitly require to be comma here, this may strike us later
		break; case Token::Type::Comma: case Token::Type::Nl:
			while (p.expect(Token::Type::Comma) || p.expect(Token::Type::Nl)) {
				sequence.file += p.consume().location(p.filename);
			}

		break; default:
			sequence.arguments.push_back(Try(parse_expression(p)));
		}
	}
endloop:

	index.file += sequence.file;
	return index;
}

Result<Ast> parse_assigment(Parser &p)
{
	log_parser_function(p);
	auto lvalue = parse_identifier(p);
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

	ensure(p.expect(std::pair{Token::Type::Operator, "="sv}), "This function should always be called with valid sequence");
	p.consume();
	return Ast::variable_declaration(lvalue->file, { *std::move(lvalue) }, Try(parse_expression(p)));
}

Result<Ast> parse_identifier(Parser &p)
{
	log_parser_function(p);

	if (not p.expect(Token::Type::Symbol)) {
		// TODO Specific error message
		return Error {
			.details = errors::internal::Unexpected_Token {
				.type = std::string(type_name(p.peek()->type)),
				.source = std::string(p.peek()->source),
				.when = "identifier parsing"
			},
			.file = p.peek()->location(p.filename)
		};
	}
	return Ast::literal(p.filename, p.consume());
}

Result<Ast> parse_note(Parser &p)
{
	log_parser_function(p);
	auto note = Ast::literal(p.filename, p.consume());
	std::optional<Ast> arg;

	bool const followed_by_fraction = p.token_id+1 < p.tokens.size()
		&& one_of(p.tokens[p.token_id+0].type, Token::Type::Symbol, Token::Type::Numeric)
		&&        p.tokens[p.token_id+1].type == Token::Type::Operator && p.tokens[p.token_id+1].source == "/"
		&& one_of(p.tokens[p.token_id+2].type, Token::Type::Symbol, Token::Type::Numeric);

	bool const followed_by_scalar = p.token_id < p.tokens.size()
		&& one_of(p.tokens[p.token_id].type, Token::Type::Symbol, Token::Type::Numeric);

	if (!(followed_by_fraction || followed_by_scalar)) {
		return note;
	}

	Ast call;
	call.type = Ast::Type::Call;
	call.file = note.file;
	call.arguments.push_back(std::move(note));

	if (followed_by_fraction) {
		auto lhs = Ast::literal(p.filename, p.consume());

		auto op = p.consume();
		ensure(op.type == Token::Type::Operator && op.source == "/", "Condition that allows entry to this block must be wrong");

		auto rhs = Ast::literal(p.filename, p.consume());

		call.arguments.push_back(Ast::binary(p.filename, std::move(op), std::move(lhs), std::move(rhs)));
	} else if (followed_by_scalar) {
		call.arguments.push_back(Ast::literal(p.filename, p.consume()));
	} else {
		unreachable();
	}

	return call;
}

Result<Ast> parse_array_literal(Parser &p)
{
	log_parser_function(p);
	auto open = p.consume();
	ensure(open.type == Token::Type::Open_Bracket, "parse_array_literal expects opening '['");

	if (p.expect(Token::Type::Close_Bracket)) {
		auto close = p.consume();
		(void)close;
		return Ast {
			.type = Ast::Type::Lazy_Array,
			.file = { .filename = p.filename, .start = open.start, .stop = close.start },
		};
	}

	auto array = Try(parse_sequence(p));
	array.type = Ast::Type::Lazy_Array;

	if (p.expect(Token::Type::Close_Bracket)) {
		auto close = p.consume();
		array .file = { .filename = p.filename, .start = open.start, .stop = close.start };
		return array;
	}

	// report an error
	unimplemented();
}

// Block parsing algorithm has awful performance, branch predictions and all this stuff
// The real culprit is a syntax probably, but this function needs to be investigated
Result<Ast> parse_paren_or_lambda(Parser &p)
{
	log_parser_function(p);
	auto open = p.consume();
	ensure(open.type == Token::Type::Open_Paren, "parse_paren_or_lambda expects that it can parse");

	if (p.expect(Token::Type::Close_Paren)) {
		return Ast::sequence({}); // () is the same as nil
	}

	bool is_lambda = false;
	std::vector<Ast> parameters;

	// Block can be either an anonymous function (lambda) or just regular sequence
	// Lambda has a parameter separator, so we try to find it first
	// parameters = { parameter, [ ',', '\n' ]* }, '|'
	{
		auto parameter_separator = p.token_id;
		std::vector<unsigned> identifier_looking;

		for (; parameter_separator < p.tokens.size(); ++parameter_separator) {
			// We want to report to the user that they provided something
			// that looks like parameter but is not in parameter definition section of a lambda
			// This is purerly to provide nice error message
			switch (p.tokens[parameter_separator].type) {
				case Token::Type::Chord:
				case Token::Type::Keyword:
					identifier_looking.push_back(parameter_separator);
					break;

				case Token::Type::Symbol:
				case Token::Type::Nl:
				case Token::Type::Comma:
					continue;

				default:
					goto endloop;
			}
		}
endloop:

		if (parameter_separator < p.tokens.size() && p.tokens[parameter_separator].type == Token::Type::Parameter_Separator) {
			// We have parameter separator, so this block really is an anonymous function
			is_lambda = true;

			// If in parameter section we have found anything that may look like identifier but really isn't then we report special error to the user
			if (identifier_looking.size()) {
				// TODO Report all instances of identifier looking parameters
				auto const& token = p.tokens[identifier_looking.front()];
				return Error {
					.details = errors::Literal_As_Identifier {
						.type_name = std::string(type_name(token.type)),
						.source    = std::string(token.source),
						.context   = "block parameter list"
					},
				};
			}

			// Consume all parameters
			while (p.token_id < parameter_separator) {
				auto token = p.consume();
				if (token.type == Token::Type::Symbol) {
					parameters.push_back(Ast::literal(p.filename, std::move(token)));
				}
			}

			// Consume parameter separator
			p.consume();
			ensure(parameter_separator+1 == p.token_id, "parameter section was not properly parsed");
		}
	}

	// If this is an anonymous function then we successfully parsed parameter section and we need to parse function body
	// If not then we are at the beggining of the block and we need to parse it
	// Both are parsed as sequence
	auto sequence = Try(parse_sequence(p));
	sequence.file.start = open.start;

	if (p.expect(Token::Type::Close_Paren)) {
		p.consume();
	} else {
		// TODO This may be a missing comma or newline error
		// Code to reach this error: (say 42)
		if (p.token_id < p.tokens.size()) {
			std::cerr << "stopped at: " << p.tokens[p.token_id] << std::endl;
		}
		unimplemented("unmatched ket");
	}

	if (is_lambda) {
		return Ast::lambda(sequence.file, std::move(sequence), std::move(parameters));
	}

	return sequence;
}

Result<Ast> parse_atomic(Parser &p)
{
	log_parser_function(p);
	auto token = Try(p.peek());
	switch (token.type) {
	case Token::Type::Keyword:
		// Not all keywords are literals. Keywords like `true` can be part of atomic expression (essentialy single value like)
		// but keywords like `var` announce variable declaration which is higher up in expression parsing.
		// So we need to explicitly allow only keywords that are also literals
		if (std::find(Literal_Keywords.begin(), Literal_Keywords.end(), token.source) == Literal_Keywords.end()) {
			return Error {
				.details = errors::Unexpected_Keyword { .keyword = std::string(p.peek()->source) },
				.file = token.location(p.filename)
			};
		}
		[[fallthrough]];
	case Token::Type::Numeric:
	case Token::Type::Symbol:
		return Ast::literal(p.filename, p.consume());

	break; case Token::Type::Chord:
		return parse_note(p);

	break; case Token::Type::Open_Bracket:
		return parse_array_literal(p);

	break; case Token::Type::Open_Paren:
		return parse_paren_or_lambda(p);

	break; default:
		std::cout << "Token at position: " << p.token_id << " with value: " << *p.peek() << std::endl;
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
	case Ast::Type::If:
		return std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin(), rhs.arguments.end());

	case Ast::Type::Literal:
		return lhs.token.type == rhs.token.type && lhs.token.source == rhs.token.source;

	case Ast::Type::Unary:
	case Ast::Type::Binary:
		return lhs.token.type == rhs.token.type
			&& lhs.token.source == rhs.token.source
			&& lhs.arguments.size() == rhs.arguments.size()
			&& std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin());

	case Ast::Type::Call:
	case Ast::Type::Lambda:
	case Ast::Type::Sequence:
	case Ast::Type::Variable_Declaration:
	case Ast::Type::Lazy_Array:
		return lhs.arguments.size() == rhs.arguments.size()
			&& std::equal(lhs.arguments.begin(), lhs.arguments.end(), rhs.arguments.begin());
	}

	unreachable();
}

std::ostream& operator<<(std::ostream& os, Ast::Type type)
{
	switch (type) {
	case Ast::Type::Binary:               return os << "BINARY";
	case Ast::Type::Call:                 return os << "CALL";
	case Ast::Type::If:                   return os << "IF";
	case Ast::Type::Lambda:               return os << "LAMBDA";
	case Ast::Type::Lazy_Array:           return os << "LAZY_ARRAY";
	case Ast::Type::Literal:              return os << "LITERAL";
	case Ast::Type::Sequence:             return os << "SEQUENCE";
	case Ast::Type::Unary:                return os << "UNARY";
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
	std::cout << "Ast::" << tree.type << "(" << tree.token.source << ")";
	switch (tree.type) {
	break; case Ast::Type::If:
		std::cout << " {\n";
		if (tree.arguments.size() >= 1) { std::cout << +i << ".cond = "; dump(tree.arguments[0], +i); }
		if (tree.arguments.size() >= 2) { std::cout << +i << ".then = "; dump(tree.arguments[1], +i); }
		if (tree.arguments.size() >= 3) { std::cout << +i << ".else = "; dump(tree.arguments[2], +i); }

	break; default:
		if (!tree.arguments.empty()) {
			std::cout << " {\n";
			for (auto const& arg : tree.arguments) {
				std::cout << +i; dump(arg, +i);
			}
			std::cout << i << '}';
		}
		std::cout << '\n';
		if (indent == 0) {
			std::cout << std::flush;
		}
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
