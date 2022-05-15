#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

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
};
