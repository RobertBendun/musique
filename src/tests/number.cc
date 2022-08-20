#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

void test_number_from(
		std::string_view source,
		Number expected,
		reflection::source_location sl = reflection::source_location::current())
{
	auto result = Number::from(Token { Token::Type::Numeric, source, {} });
	expect(result.has_value(), sl) << "failed to parse number";
	if (result.has_value()) {
		expect(eq(*result, expected), sl);
	}
}

suite number_test = [] {
	"Number"_test = [] {
		should("provide arithmetic operators") = [] {
			expect(eq(Number{1, 8} + Number{3, 4}, Number{ 7,  8})) << "for expr: (1/8) + (3/4)";
			expect(eq(Number{1, 8} - Number{3, 4}, Number{-5,  8})) << "for expr: (1/8) - (3/4)";
			expect(eq(Number{1, 8} * Number{3, 4}, Number{ 3, 32})) << "for expr: (1/8) * (3/4)";
			expect(eq(Number{1, 8} / Number{3, 4}, Number{ 1,  6})) << "for expr: (1/8) / (3/4)";
		};

		should("provide assignment operators") = [] {
			Number n;
			n={1,8}; n += {3,4}; expect(eq(n, Number{ 7,  8})) << "for expr: (1/8) += (3/4)";
			n={1,8}; n -= {3,4}; expect(eq(n, Number{-5,  8})) << "for expr: (1/8) -= (3/4)";
			n={1,8}; n *= {3,4}; expect(eq(n, Number{ 3, 32})) << "for expr: (1/8) *= (3/4)";
			n={1,8}; n /= {3,4}; expect(eq(n, Number{ 1,  6})) << "for expr: (1/8) /= (3/4)";
		};

		should("be comperable") = [] {
			expect(gt(Number{1, 4}, Number{1, 8}));
			expect(eq(Number{2, 4}, Number{1, 2}));
		};

		should("be convertable to int") = [] {
			expect(eq(Number{4, 2}.as_int(), 2))    << "for fraction 4/2";
			expect(eq(Number{2, 1}.as_int(), 2))    << "for fraction 2/1";
			expect(eq(Number{0, 1000}.as_int(), 0)) << "for fraction 0/1000";
		};
	};

	"Number::from"_test = [] {
		test_number_from("0",    Number(0));
		test_number_from("100",  Number(100));
		test_number_from("0.75", Number(3, 4));
		test_number_from(".75",  Number(3, 4));
		test_number_from("120.", Number(120, 1));
	};

	"Rounding"_test = [] {
		should("Support floor operation") = [] {
			expect(eq(Number(0),   Number(1, 2).floor()));
			expect(eq(Number(1),   Number(3, 2).floor()));
			expect(eq(Number(-1),  Number(-1, 2).floor()));
			expect(eq(Number(0),   Number(0).floor()));
			expect(eq(Number(1),   Number(1).floor()));
			expect(eq(Number(-1),  Number(-1).floor()));
		};

		should("Support ceil operation") = [] {
			expect(eq(Number(1),   Number(1, 2).ceil()));
			expect(eq(Number(2),   Number(3, 2).ceil()));
			expect(eq(Number(0),  Number(-1, 2).ceil()));
			expect(eq(Number(-1),  Number(-3, 2).ceil()));
			expect(eq(Number(0),   Number(0).ceil()));
			expect(eq(Number(1),   Number(1).ceil()));
			expect(eq(Number(-1),  Number(-1).ceil()));
		};

		should("Support round operation") = [] {
			expect(eq(Number(1),   Number(3, 4).round()));
			expect(eq(Number(0),   Number(1, 4).round()));
			expect(eq(Number(1),   Number(5, 4).round()));
			expect(eq(Number(2),   Number(7, 4).round()));

			expect(eq(Number(-1),   Number(-3, 4).round()));
			expect(eq(Number(0),   Number(-1, 4).round()));
			expect(eq(Number(-1),   Number(-5, 4).round()));
			expect(eq(Number(-2),   Number(-7, 4).round()));
		};
	};

	"Number exponantiation"_test = [] {
		should("Support integer powers") = [] {
			expect(eq(Number(1, 4), Number(1, 2).pow(Number(2))));
			expect(eq(Number(4, 1), Number(1, 2).pow(Number(-2))));

			expect(eq(Number(4, 1), Number(2).pow(Number(2))));
			expect(eq(Number(1, 4), Number(2).pow(Number(-2))));
		};
	};
};
