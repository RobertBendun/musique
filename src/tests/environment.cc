#if 0
#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

static void equals(
	Value *received,
	Value expected,
	reflection::source_location sl = reflection::source_location::current())
{
	expect(received != nullptr, sl) << "Value was not found";
	expect(eq(*received, expected), sl);
}

suite environment_test = [] {
	"Global enviroment exists"_test = [] {
		Interpreter i;
		expect(eq(&i.env_pool.front(), &Env::global()));
	};

	"Environment scoping"_test = [] {
		should("nested scoping preserve outer scope") = [] {
			Interpreter i;

			i.env().force_define("x", Value::number(Number(10)));
			i.env().force_define("y", Value::number(Number(20)));

			equals(i.env().find("x"), Value::number(Number(10)));
			equals(i.env().find("y"), Value::number(Number(20)));

			i.current_env = ++i.env();
			i.env().force_define("x", Value::number(Number(30)));
			equals(i.env().find("x"), Value::number(Number(30)));
			equals(i.env().find("y"), Value::number(Number(20)));

			i.current_env = --i.env();
			equals(i.env().find("x"), Value::number(Number(10)));
			equals(i.env().find("y"), Value::number(Number(20)));
		};

		should("nested variables missing from outer scope") = [] {
			Interpreter i;

			i.current_env = ++i.env();
			i.env().force_define("x", Value::number(Number(30)));
			equals(i.env().find("x"), Value::number(Number(30)));

			i.current_env = --i.env();
			expect(eq(i.env().find("x"), nullptr));
		};

		should("stay at global scope when too much end of scope is applied") = [] {
			Interpreter i;
			i.current_env = --i.env();
			expect(eq(&i.env(), &Env::global()));
		};

		should("reuse already allocated enviroments") = [] {
			Interpreter i;

			i.current_env = ++i.env();
			auto const first_env = &i.env();
			i.env().force_define("x", Value::number(Number(30)));
			equals(i.env().find("x"), Value::number(Number(30)));

			i.current_env = --i.env();
			expect(eq(i.env().find("x"), nullptr));

			i.current_env = ++i.env();
			expect(eq(first_env, &i.env()));
			expect(eq(i.env().find("x"), nullptr));
			i.env().force_define("x", Value::number(Number(30)));
			equals(i.env().find("x"), Value::number(Number(30)));

			i.current_env = --i.env();
			expect(eq(i.env().find("x"), nullptr));
		};
	};
};
#endif
