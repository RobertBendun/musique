#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;

void expect_ast(
		std::string_view source,
		Ast const& expected,
		reflection::source_location sl = reflection::source_location::current())
{
	auto result = Parser::parse(source, "test");
	expect(result.has_value(), sl) << "code was expect to parse, but had not";
	expect(eq(*result, expected)) << "parser yielded unexpected tree";
}

suite parser_test = [] {
	"Literal parsing"_test = [] {
		expect_ast("1", Ast::literal(Token { Token::Type::Numeric, "1", {} }));
	};

	"Binary opreator parsing"_test = [] {
		expect_ast("1 + 2", Ast::binary(
				Token { Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Numeric, "1", {} }),
				Ast::literal({ Token::Type::Numeric, "2", {} })
		));

		expect_ast("100 * 200", Ast::binary(
				Token { Token::Type::Operator, "*", {} },
				Ast::literal({ Token::Type::Numeric, "100", {} }),
				Ast::literal({ Token::Type::Numeric, "200", {} })
		));

		expect_ast("101 + 202 * 303", Ast::binary(
				Token { Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Numeric, "101", {} }),
				Ast::binary(
							Token { Token::Type::Operator, "*", {} },
							Ast::literal({ Token::Type::Numeric, "202", {} }),
							Ast::literal({ Token::Type::Numeric, "303", {} })
				)
		));

	};

	// This test shouldn't be skipped since language will support precedense.
	// It stays here as reminder that this feature should be implemented.
	skip / "Binary operator precedense"_test = [] {
		expect_ast("101 * 202 + 303", Ast::binary(
				Token { Token::Type::Operator, "+", {} },
				Ast::binary(
							Token { Token::Type::Operator, "*", {} },
							Ast::literal({ Token::Type::Numeric, "101", {} }),
							Ast::literal({ Token::Type::Numeric, "202", {} })
				),
				Ast::literal({ Token::Type::Numeric, "303", {} })
		));
	};
};
