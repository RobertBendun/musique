#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

static std::string str(auto const& printable)
{
	std::stringstream ss;
	ss << printable;
	return std::move(ss).str();
}

suite value_test = [] {
	"Comparisons"_test = [] {
		should("be always not equal for lambdas") = [] {
			expect(neq(Value::lambda(nullptr), Value::lambda(nullptr)));
		};

		should("are always not equal when types differ") = [] {
			expect(neq(Value::symbol("0"), Value::number(Number(0))));
		};
	};

	"Value printing"_test = [] {
		expect(eq("nil"sv, str(Value{})));

		expect(eq("10"sv, str(Value::number(Number(10)))));
		expect(eq("1/2"sv, str(Value::number(Number(2, 4)))));

		expect(eq("foo"sv, str(Value::symbol("foo"))));

		expect(eq("<lambda>"sv, str(Value::lambda(nullptr))));
	};
};
