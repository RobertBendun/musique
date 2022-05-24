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
		expect(not bool(Env::global)) << "Before interpreter global environment does not exists";
		{
			Interpreter i;
			expect(bool(Env::global)) << "When interpreter exists global environment exists";
		}
		expect(not bool(Env::global)) << "After interpreter destruction, global environment does not exists";
	};

	"Environment scoping"_test = [] {
		should("nested scoping preserve outer scope") = [] {
			Interpreter i;

			i.env->force_define("x", Value::from(Number(10)));
			i.env->force_define("y", Value::from(Number(20)));

			equals(i.env->find("x"), Value::from(Number(10)));
			equals(i.env->find("y"), Value::from(Number(20)));

			i.enter_scope();
			{
				i.env->force_define("x", Value::from(Number(30)));
				equals(i.env->find("x"), Value::from(Number(30)));
				equals(i.env->find("y"), Value::from(Number(20)));
			}
			i.leave_scope();

			equals(i.env->find("x"), Value::from(Number(10)));
			equals(i.env->find("y"), Value::from(Number(20)));
		};

		should("nested variables missing from outer scope") = [] {
			Interpreter i;

			i.enter_scope();
			{
				i.env->force_define("x", Value::from(Number(30)));
				equals(i.env->find("x"), Value::from(Number(30)));
			}
			i.leave_scope();

			expect(eq(i.env->find("x"), nullptr));
		};
	};
};
