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
	"Empty file parsing"_test = [] {
		expect_ast("", Ast::sequence({}));
	};

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

	"Grouping expressions in parentheses"_test = [] {
		expect_ast("(101 + 202) * 303", Ast::binary(
				Token { Token::Type::Operator, "*", {} },
				Ast::binary(
							Token { Token::Type::Operator, "+", {} },
							Ast::literal({ Token::Type::Numeric, "101", {} }),
							Ast::literal({ Token::Type::Numeric, "202", {} })
				),
				Ast::literal({ Token::Type::Numeric, "303", {} })
		));

		expect_ast("101 * (202 + 303)", Ast::binary(
				Token { Token::Type::Operator, "*", {} },
				Ast::literal({ Token::Type::Numeric, "101", {} }),
				Ast::binary(
							Token { Token::Type::Operator, "+", {} },
							Ast::literal({ Token::Type::Numeric, "202", {} }),
							Ast::literal({ Token::Type::Numeric, "303", {} })
				)
		));
	};

	"Explicit function call"_test = [] {
		expect_ast("foo 1 2", Ast::call({
			Ast::literal({ Token::Type::Symbol, "foo", {} }),
			Ast::literal({ Token::Type::Numeric, "1", {} }),
			Ast::literal({ Token::Type::Numeric, "2", {} })
		}));

		expect_ast("say (fib (n-1) + fib (n-2))", Ast::call({
			Ast::literal({ Token::Type::Symbol, "say", {} }),
			Ast::binary(
				{ Token::Type::Operator, "+", {} },
				Ast::call({
					Ast::literal({ Token::Type::Symbol, "fib", {} }),
					Ast::binary(
						{ Token::Type::Operator, "-", {} },
						Ast::literal({ Token::Type::Symbol, "n", {} }),
						Ast::literal({ Token::Type::Numeric, "1", {} })
					)
				}),
				Ast::call({
					Ast::literal({ Token::Type::Symbol, "fib", {} }),
					Ast::binary(
						{ Token::Type::Operator, "-", {} },
						Ast::literal({ Token::Type::Symbol, "n", {} }),
						Ast::literal({ Token::Type::Numeric, "2", {} })
					)
				})
			)
		}));
	};

	"Sequence"_test = [] {
		expect_ast("42; 101", Ast::sequence({
			Ast::literal({ Token::Type::Numeric, "42", {} }),
			Ast::literal({ Token::Type::Numeric, "101", {} })
		}));

		expect_ast("say hello; say world", Ast::sequence({
			Ast::call({
				Ast::literal({ Token::Type::Symbol, "say", {} }),
				Ast::literal({ Token::Type::Symbol, "hello", {} })
			}),
			Ast::call({
				Ast::literal({ Token::Type::Symbol, "say", {} }),
				Ast::literal({ Token::Type::Symbol, "world", {} })
			})
		}));
	};

	"Block"_test = [] {
		expect_ast("[]", Ast::block(Location{}));

		expect_ast("[ i; j; k ]", Ast::block({}, Ast::sequence({
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::literal({ Token::Type::Symbol, "j", {} }),
			Ast::literal({ Token::Type::Symbol, "k", {} })
		})));

		expect_ast("[ i j k | i + j + k ]", Ast::lambda({}, Ast::binary(
			{ Token::Type::Operator, "+", {} },
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::binary(
				{ Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Symbol, "j", {} }),
				Ast::literal({ Token::Type::Symbol, "k", {} })
			)
		), {
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::literal({ Token::Type::Symbol, "j", {} }),
			Ast::literal({ Token::Type::Symbol, "k", {} })
		}));

		expect_ast("[ i; j; k | i + j + k ]", Ast::lambda({}, Ast::binary(
			{ Token::Type::Operator, "+", {} },
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::binary(
				{ Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Symbol, "j", {} }),
				Ast::literal({ Token::Type::Symbol, "k", {} })
			)
		), {
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::literal({ Token::Type::Symbol, "j", {} }),
			Ast::literal({ Token::Type::Symbol, "k", {} })
		}));

		expect_ast("[|1]", Ast::lambda({}, Ast::literal({ Token::Type::Numeric, "1", {} }), {}));
	};

	"Variable declarations"_test = [] {
		expect_ast("var x = 10", Ast::variable_declaration(
			{},
			{ Ast::literal({ Token::Type::Symbol, "x", {} }) },
			Ast::literal({ Token::Type::Numeric, "10", {} })));

		expect_ast("var x", Ast::variable_declaration(
			{},
			{ Ast::literal({ Token::Type::Symbol, "x", {} }) },
			std::nullopt));

		expect_ast("var x y", Ast::variable_declaration(
			{},
			{Ast::literal({ Token::Type::Symbol, "x", {} }), Ast::literal({ Token::Type::Symbol, "y", {} })},
			std::nullopt));
	};
};
