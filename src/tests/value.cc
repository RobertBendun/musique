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

static void expect_value(
	Result<Value> received,
	Value expected,
	reflection::source_location sl = reflection::source_location::current())
{
	expect(received.has_value(), sl) << "Received error, instead of value";
	expect(eq(*received, expected), sl);
}

static void either_truthy_or_falsy(
	bool(Value::*desired)() const,
	Value v,
	reflection::source_location sl = reflection::source_location::current())
{
	if (desired == &Value::truthy) {
		expect(v.truthy(), sl) << "Value " << v << " should be"     << "truthy";
		expect(!v.falsy(), sl) << "Value " << v << " should NOT be" << "falsy";
	} else {
		expect(v.falsy(), sl)   << "Value " << v << " should be"     << "falsy";
		expect(!v.truthy(), sl) << "Value " << v << " should NOT be" << "truthy";
	}
}

suite value_test = [] {
	"Value"_test = [] {
		should("be properly created using Value::from") = [] {
			expect_value(Value::from({ Token::Type::Numeric, "10", {} }),     Value::number(Number(10)));
			expect_value(Value::from({ Token::Type::Keyword, "nil", {} }),    Value{});
			expect_value(Value::from({ Token::Type::Keyword, "true", {} }),   Value::boolean(true));
			expect_value(Value::from({ Token::Type::Keyword, "false", {} }),  Value::boolean(false));
			expect_value(Value::from({ Token::Type::Symbol,  "foobar", {} }), Value::symbol("foobar"));
		};

		should("have be considered truthy or falsy") = [] {
			either_truthy_or_falsy(&Value::truthy, Value::boolean(true));
			either_truthy_or_falsy(&Value::truthy, Value::number(Number(1)));
			either_truthy_or_falsy(&Value::truthy, Value::symbol("foo"));
			either_truthy_or_falsy(&Value::truthy, Value(Intrinsic(nullptr)));

			either_truthy_or_falsy(&Value::falsy, Value{});
			either_truthy_or_falsy(&Value::falsy, Value::boolean(false));
			either_truthy_or_falsy(&Value::falsy, Value::number(Number(0)));
		};
	};

	"Value comparisons"_test = [] {
		should("are always not equal when types differ") = [] {
			expect(neq(Value::symbol("0"), Value::number(Number(0))));
		};
	};

	"Value printing"_test = [] {
		expect(eq("nil"sv, str(Value{})));
		expect(eq("true"sv, str(Value::boolean(true))));
		expect(eq("false"sv, str(Value::boolean(false))));

		expect(eq("10"sv, str(Value::number(Number(10)))));
		expect(eq("1/2"sv, str(Value::number(Number(2, 4)))));

		expect(eq("foo"sv, str(Value::symbol("foo"))));

		expect(eq("<intrinsic>"sv, str(Value(Intrinsic(nullptr)))));
	};
};
