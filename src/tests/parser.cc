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
	expect(eq(*result, expected), sl) << "parser yielded unexpected tree";
}

void expect_single_ast(
		std::string_view source,
		Ast const& expected,
		reflection::source_location sl = reflection::source_location::current())
{
	auto result = Parser::parse(source, "test");
	expect(result.has_value(), sl) << "code was expect to parse, but had not";

	expect(eq(result->type, Ast::Type::Sequence), sl) << "parsed does not yielded sequence";
	expect(not result->arguments.empty(), sl) << "parsed yielded empty sequence";
	expect(eq(result->arguments[0], expected), sl) << "parser yielded unexpected tree";
}

suite parser_test = [] {
	"Empty file parsing"_test = [] {
		expect_ast("", Ast::sequence({}));
	};

	"Literal parsing"_test = [] {
		expect_single_ast("1", Ast::literal(Token { Token::Type::Numeric, "1", {} }));
	};

	"Binary opreator parsing"_test = [] {
		expect_single_ast("1 + 2", Ast::binary(
				Token { Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Numeric, "1", {} }),
				Ast::literal({ Token::Type::Numeric, "2", {} })
		));

		expect_single_ast("100 * 200", Ast::binary(
				Token { Token::Type::Operator, "*", {} },
				Ast::literal({ Token::Type::Numeric, "100", {} }),
				Ast::literal({ Token::Type::Numeric, "200", {} })
		));

		expect_single_ast("101 + 202 * 303", Ast::binary(
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
	"Binary operator precedense"_test = [] {
		expect_single_ast("101 * 202 + 303", Ast::binary(
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
		expect_single_ast("(101 + 202) * 303", Ast::binary(
				Token { Token::Type::Operator, "*", {} },
				Ast::sequence({ Ast::binary(
							Token { Token::Type::Operator, "+", {} },
							Ast::literal({ Token::Type::Numeric, "101", {} }),
							Ast::literal({ Token::Type::Numeric, "202", {} })
				)}),
				Ast::literal({ Token::Type::Numeric, "303", {} })
		));

		expect_single_ast("101 * (202 + 303)", Ast::binary(
				Token { Token::Type::Operator, "*", {} },
				Ast::literal({ Token::Type::Numeric, "101", {} }),
				Ast::sequence({
					Ast::binary(
								Token { Token::Type::Operator, "+", {} },
								Ast::literal({ Token::Type::Numeric, "202", {} }),
								Ast::literal({ Token::Type::Numeric, "303", {} })
					)
				})
		));
	};

	"Explicit function call"_test = [] {
		expect_single_ast("foo 1 2", Ast::call({
			Ast::literal({ Token::Type::Symbol, "foo", {} }),
			Ast::literal({ Token::Type::Numeric, "1", {} }),
			Ast::literal({ Token::Type::Numeric, "2", {} })
		}));

		should("Prioritize binary expressions over function calls") = [] {
			expect_single_ast("say 1 + 2", Ast::call({
				Ast::literal({ Token::Type::Symbol, "say", {} }),
				Ast::binary({ Token::Type::Operator, "+", {} },
					Ast::literal({ Token::Type::Numeric, "1", {} }),
					Ast::literal({ Token::Type::Numeric, "2", {} }))
			}));
		};

		should("Support function call with complicated expression") = [] {
			expect_single_ast("say (fib (n-1) + fib (n-2))", Ast::call({
				Ast::literal({ Token::Type::Symbol, "say", {} }),
				Ast::sequence({
					Ast::binary(
						{ Token::Type::Operator, "+", {} },
						Ast::call({
							Ast::literal({ Token::Type::Symbol, "fib", {} }),
							Ast::sequence({
								Ast::binary(
									{ Token::Type::Operator, "-", {} },
									Ast::literal({ Token::Type::Symbol, "n", {} }),
									Ast::literal({ Token::Type::Numeric, "1", {} })
								)
							})
						}),
						Ast::call({
							Ast::literal({ Token::Type::Symbol, "fib", {} }),
							Ast::sequence({
								Ast::binary(
									{ Token::Type::Operator, "-", {} },
									Ast::literal({ Token::Type::Symbol, "n", {} }),
									Ast::literal({ Token::Type::Numeric, "2", {} })
								)
							})
						})
					)
				})
			}));
		};
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
		expect_single_ast("[]", Ast::block(Location{}));

		expect_single_ast("[ i; j; k ]", Ast::block({}, Ast::sequence({
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::literal({ Token::Type::Symbol, "j", {} }),
			Ast::literal({ Token::Type::Symbol, "k", {} })
		})));

		expect_single_ast("[ i j k | i + j + k ]", Ast::lambda({}, Ast::sequence({
			Ast::binary(
				{ Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Symbol, "i", {} }),
					Ast::binary(
						{ Token::Type::Operator, "+", {} },
						Ast::literal({ Token::Type::Symbol, "j", {} }),
						Ast::literal({ Token::Type::Symbol, "k", {} })
					)
				)
			})
		, {
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::literal({ Token::Type::Symbol, "j", {} }),
			Ast::literal({ Token::Type::Symbol, "k", {} })
		}));

		expect_single_ast("[ i; j; k | i + j + k ]", Ast::lambda({}, Ast::sequence({
			Ast::binary(
				{ Token::Type::Operator, "+", {} },
				Ast::literal({ Token::Type::Symbol, "i", {} }),
				Ast::binary(
					{ Token::Type::Operator, "+", {} },
					Ast::literal({ Token::Type::Symbol, "j", {} }),
					Ast::literal({ Token::Type::Symbol, "k", {} })
				)
		)}), {
			Ast::literal({ Token::Type::Symbol, "i", {} }),
			Ast::literal({ Token::Type::Symbol, "j", {} }),
			Ast::literal({ Token::Type::Symbol, "k", {} })
		}));

		expect_single_ast("[|1]", Ast::lambda({}, Ast::sequence({ Ast::literal({ Token::Type::Numeric, "1", {} })}), {}));
	};

	"Variable declarations"_test = [] {
		should("Support variable declaration with assigment") = [] {
			expect_single_ast("var x = 10", Ast::variable_declaration(
				{},
				{ Ast::literal({ Token::Type::Symbol, "x", {} }) },
				Ast::literal({ Token::Type::Numeric, "10", {} })));
		};

		skip / should("Support variable declaration") = [] {
			expect_ast("var x", Ast::variable_declaration(
				{},
				{ Ast::literal({ Token::Type::Symbol, "x", {} }) },
				std::nullopt));
		};

		skip / should("Support multiple variables declaration") = [] {
			expect_ast("var x y", Ast::variable_declaration(
				{},
				{Ast::literal({ Token::Type::Symbol, "x", {} }), Ast::literal({ Token::Type::Symbol, "y", {} })},
				std::nullopt));
		};
	};
};
