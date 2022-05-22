#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

static auto capture_errors(auto lambda, reflection::source_location sl = reflection::source_location::current())
{
	return [=] {
		auto result = lambda();
		if (not result.has_value()) {
			expect(false, sl) << "failed test with error: " << result.error();
		}
	};
}

void evaluates_to(Value value, std::string_view source_code, reflection::source_location sl = reflection::source_location::current())
{
	capture_errors([=]() -> Result<void> {
		Interpreter interpreter;
		auto result = Try(interpreter.eval(Try(Parser::parse(source_code, "test"))));
		expect(eq(result, value));
		return {};
	}, sl)();
}

void produces_output(std::string_view source_code, std::string_view expected_output, reflection::source_location sl = reflection::source_location::current())
{
	capture_errors([=]() -> Result<void> {
		std::stringstream ss;
		Interpreter interpreter(ss);
		Try(interpreter.eval(Try(Parser::parse(source_code, "test"))));
		expect(eq(ss.str(), expected_output)) << "different evaluation output";
		return {};
	}, sl)();
}

suite intepreter_test = [] {
	"Interpreter"_test = [] {
		should("evaluate literals") = [] {
			evaluates_to(Value{}, "nil");
			evaluates_to(Value::number(Number(10)), "10");
			// evaluates_to(Value::symbol("notexistingsymbol"), "notexistingsymbol");
		};

		should("evaluate arithmetic") = [] {
			evaluates_to(Value::number(Number(10)), "5 + 3 + 2");
			evaluates_to(Value::number(Number(25)), "5 * (3 + 2)");
			evaluates_to(Value::number(Number(1, 2)), "1 / 2");
			evaluates_to(Value::number(Number(-10)), "10 - 20");
		};

		should("call builtin functions") = [] {
			evaluates_to(Value::symbol("nil"),     "typeof nil");
			evaluates_to(Value::symbol("number"),  "typeof 100");

			produces_output("say 5", "5\n");
			produces_output("say 1; say 2; say 3", "1\n2\n3\n");
			produces_output("say 1 2 3", "1 2 3\n");
		};

		should("allows only for calling which is callable") = [] {
			Interpreter i;
			{
				auto result = Parser::parse("10 20", "test").and_then([&](Ast &&ast) { return i.eval(std::move(ast)); });
				expect(!result.has_value()) << "Expected code to have failed";
				expect(eq(result.error().type, errors::Not_Callable));
			}
			{
				i.env->force_define("call_me", Value::number(Number(10)));
				auto result = Parser::parse("call_me 20", "test").and_then([&](Ast &&ast) { return i.eval(std::move(ast)); });
				expect(!result.has_value()) << "Expected code to have failed";
				expect(eq(result.error().type, errors::Not_Callable));
			}
		};

		// Added to explicitly test against bug that was in old implementation of enviroments.
		// Previously this test would segfault
		should("allow assigning result of function calls to a variable") = [] {
			evaluates_to(Value::number(Number(42)), "var x = [i|i] 42; x");
		};
	};
};
