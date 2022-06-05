#include <boost/ut.hpp>
#include <musique.hh>

#include <algorithm>
#include <numeric>

using namespace boost::ut;
using namespace std::string_view_literals;

static auto capture_errors(auto lambda, reflection::source_location sl = reflection::source_location::current())
{
	return [=] {
		auto result = lambda();
		if (not result.has_value()) {
			expect(false, sl) << "failed test with error: " << result.error();
			return false;
		}
		return true;
	};
}

auto evaluates_to(Value value, std::string_view source_code, reflection::source_location sl = reflection::source_location::current())
{
	return capture_errors([=]() -> Result<void> {
		Interpreter interpreter;
		auto result = Try(interpreter.eval(Try(Parser::parse(source_code, "test"))));
		expect(eq(result, value), sl);
		return {};
	}, sl)();
}

template<typename T>
void expect_alternative(
	auto const& variant,
	boost::ut::reflection::source_location sl = boost::ut::reflection::source_location::current())
{
	using namespace boost::ut;
	expect(std::holds_alternative<T>(variant), sl) << "Expected to hold alternative but failed";
}

suite intepreter_test = [] {
	"Interpreter"_test = [] {
		should("evaluate literals") = [] {
			evaluates_to(Value::from(false),      "false");
			evaluates_to(Value::from(true),       "true");
			evaluates_to(Value::from(Number(10)), "10");
			evaluates_to(Value{},                 "nil");
		};

		should("evaluate arithmetic") = [] {
			evaluates_to(Value::from(Number(10)), "5 + 3 + 2");
			evaluates_to(Value::from(Number(25)), "5 * (3 + 2)");
			evaluates_to(Value::from(Number(1, 2)), "1 / 2");
			evaluates_to(Value::from(Number(-10)), "10 - 20");
		};

		should("call builtin functions") = [] {
			evaluates_to(Value::from("nil"),     "typeof nil");
			evaluates_to(Value::from("number"),  "typeof 100");
		};

		should("allows only for calling which is callable") = [] {
			evaluates_to(Value::from(Number(0)), "[i|i] 0");
			{
				Interpreter i;
				{
					auto result = Parser::parse("10 20", "test").and_then([&](Ast &&ast) { return i.eval(std::move(ast)); });
					expect(!result.has_value()) << "Expected code to have failed";
					expect_alternative<errors::Not_Callable>(result.error().details);
				}
				{
					i.env->force_define("call_me", Value::from(Number(10)));
					auto result = Parser::parse("call_me 20", "test").and_then([&](Ast &&ast) { return i.eval(std::move(ast)); });
					expect(!result.has_value()) << "Expected code to have failed";
					expect_alternative<errors::Not_Callable>(result.error().details);
				}
			}
		};

		should("allow for value (in)equality comparisons") = [] {
			evaluates_to(Value::from(true),  "nil == nil");
			evaluates_to(Value::from(false), "nil != nil");

			evaluates_to(Value::from(true),  "true == true");
			evaluates_to(Value::from(false), "true != true");
			evaluates_to(Value::from(false), "true == false");
			evaluates_to(Value::from(true),  "true != false");

			evaluates_to(Value::from(true),  "0 == 0");
			evaluates_to(Value::from(false), "0 != 0");
			evaluates_to(Value::from(true),  "1 != 0");
			evaluates_to(Value::from(false), "1 == 0");
		};

		should("allow for value ordering comparisons") = [] {
			evaluates_to(Value::from(false), "true <  true");
			evaluates_to(Value::from(true),  "true <= true");
			evaluates_to(Value::from(true),  "false < true");
			evaluates_to(Value::from(false), "false > true");

			evaluates_to(Value::from(false), "0 <  0");
			evaluates_to(Value::from(true),  "0 <= 0");
			evaluates_to(Value::from(true),  "1 <  2");
			evaluates_to(Value::from(false), "1 >  2");

			evaluates_to(Value::from(true),  "c == c");
			evaluates_to(Value::from(false), "c != c");

			evaluates_to(Value::from(true),  "c < d");
			evaluates_to(Value::from(true),  "c != (c 4)");
			evaluates_to(Value::from(true),  "(c 4) == (c 4)");
			evaluates_to(Value::from(true),  "(c 3) != (c 4)");
			evaluates_to(Value::from(true),  "(c 3) < (c 4)");
			evaluates_to(Value::from(true),  "((c+12) 3) == (c 4)");

			// Value is partially ordered, ensure that different types
			// are always not equal and not ordered
			evaluates_to(Value::from(false), "0 <  c");
			evaluates_to(Value::from(false), "0 >  c");
			evaluates_to(Value::from(false), "0 <= c");
			evaluates_to(Value::from(false), "0 >= c");
			evaluates_to(Value::from(true),  "0 != c");
		};

		// Added to explicitly test against bug that was in old implementation of enviroments.
		// Previously this test would segfault
		should("allow assigning result of function calls to a variable") = [] {
			evaluates_to(Value::from(Number(42)), "var x = [i|i] 42; x");
		};

		should("support array programming") = [] {
			evaluates_to(Value::from(Array { .elements = {{
				Value::from(Number(2)),
				Value::from(Number(4)),
				Value::from(Number(6)),
				Value::from(Number(8))
			}}}), "2 * [1;2;3;4]");

			evaluates_to(Value::from(Array { .elements = {{
				Value::from(Note { .base = 1 }),
				Value::from(Note { .base = 2 }),
				Value::from(Note { .base = 3 }),
				Value::from(Note { .base = 4 })
			}}}), "c + [1;2;3;4]");

			evaluates_to(Value::from(Array { .elements = {{
				Value::from(Number(2)),
				Value::from(Number(4)),
				Value::from(Number(6)),
				Value::from(Number(8))
			}}}), "[1;2;3;4] * 2");

			evaluates_to(Value::from(Array { .elements = {{
				Value::from(Note { .base = 1 }),
				Value::from(Note { .base = 2 }),
				Value::from(Note { .base = 3 }),
				Value::from(Note { .base = 4 })
			}}}), "[1;2;3;4] + c");
		};

		should("support number - music operations") = [] {
			evaluates_to(Value::from(Chord { .notes = { Note { .base = 0 }, Note { .base = 3 }, Note { .base = 6 } } }),
				"c#47 - 1");

			evaluates_to(Value::from(Chord { .notes = { Note { .base = 2 }, Note { .base = 6 }, Note { .base = 9 } } }),
				"c47 + 2");

			evaluates_to(Value::from(Chord { .notes = { Note { .base = 2 }, Note { .base = 6 }, Note { .base = 9 } } }),
				"2 + c47");
		};

		should("support direct chord creation") = [] {
			evaluates_to(Value::from(Chord { .notes = { Note { .base = 0 }, Note { .base = 4 }, Note { .base = 7 } } }),
				"chord [c;e;g]");

			evaluates_to(Value::from(Chord { .notes = { Note { .base = 0 }, Note { .base = 4 }, Note { .base = 7 } } }),
				"chord c e g");

			evaluates_to(Value::from(Chord { .notes = { Note { .base = 0 }, Note { .base = 4 }, Note { .base = 7 } } }),
				"chord [c;e] g");
		};

		should("support eager array creation") = [] {
			evaluates_to(Value::from(Array { .elements = {{ Value::from(Number(10)), Value::from(Number(20)), Value::from(Number(30)) }}}),
				"flat 10 20 30");

			evaluates_to(Value::from(Array { .elements = {{ Value::from(Number(10)), Value::from(Number(20)), Value::from(Number(30)) }}}),
				"flat [10;20] 30");

			evaluates_to(Value::from(Array { .elements = {{ Value::from(Number(10)), Value::from(Number(20)), Value::from(Number(30)) }}}),
				"flat (flat 10 20) 30");
		};

		should("support indexing") = [] {
			evaluates_to(Value::from(Number(10)), "[5;10;15].1");
			evaluates_to(Value::from(Number(10)), "(flat 5 10 15).1");
		};

		should("support size") = [] {
			evaluates_to(Value::from(Number(3)), "len [5;10;15]");
			evaluates_to(Value::from(Number(3)), "len (flat 5 10 15)");
		};

		should("support call like octave and len setting") = [] {
			evaluates_to(Value::from(Chord { .notes = {{ Note { .base = 0, .octave = 5, .length = Number(10) }}}}), "c 5 10");
			evaluates_to(Value::from(Chord { .notes = {{ Note { .base = 0, .octave = 5, .length = Number(10) }}}}), "(c 4 8) 5 10");
			evaluates_to(Value::from(Chord { .notes = {{ Note { .base = 0, .octave = 5 }}}}), "c 5");
		};
	};

	"Interpreter boolean operators"_test = [] {
		should("Properly support 'and' truth table") = [] {
			evaluates_to(Value::from(false), "false and false");
			evaluates_to(Value::from(false), "true  and false");
			evaluates_to(Value::from(false), "false and true");
			evaluates_to(Value::from(true),  "true and true");

			evaluates_to(Value::from(Number(0)),  "0 and 0");
			evaluates_to(Value::from(Number(0)),  "0 and 1");
			evaluates_to(Value::from(Number(0)),  "1 and 0");
			evaluates_to(Value::from(Number(1)),  "1 and 1");
			evaluates_to(Value::from(Number(42)), "1 and 42");
		};

		should("Properly support 'or' truth table") = [] {
			evaluates_to(Value::from(false), "false or false");
			evaluates_to(Value::from(true),  "true  or false");
			evaluates_to(Value::from(true),  "false or true");
			evaluates_to(Value::from(true),  "true  or true");

			evaluates_to(Value::from(Number(0)),  "0 or 0");
			evaluates_to(Value::from(Number(1)),  "0 or 1");
			evaluates_to(Value::from(Number(1)),  "1 or 0");
			evaluates_to(Value::from(Number(1)),  "1 or 1");
			evaluates_to(Value::from(Number(1)),  "1 or 42");
			evaluates_to(Value::from(Number(42)), "42 or 1");
		};

		// TODO Implement this test
		// Currently not implemented due to interpeter inability to accept
		// stateful intrinsics. Probably needs to be solved with using in-language
		// variable like "__called_times_" or using names that cannot be variables
		// like "    ". But I need some time to think about that
		skip / should("Short circuit on 'and' and 'or' operators") = [] {
		};
	};

	"Interpreter's default builtins"_test = [] {
		should("Conditional execution with 'if'") = [] {
			evaluates_to(Value::from(Number(10)), "if true  [10] [20]");
			evaluates_to(Value::from(Number(20)), "if false [10] [20]");
			evaluates_to(Value{},                 "if false [10]");
		};

		should("Permutations generation with 'permute'") = [] {
			std::vector<int> src(5);
			std::iota(src.begin(), src.end(), 0);

			auto exp = Value::from(Array{});
			exp.array.elements.resize(src.size());

			for (bool quit = false; !quit;) {
				std::stringstream ss;
				ss << "permute ";
				std::copy(src.begin(), src.end(), std::ostream_iterator<int>(ss, " "));

				quit = not std::next_permutation(src.begin(), src.end());

				std::transform(src.begin(), src.end(), exp.array.elements.begin(), [](int x) {
					return Value::from(Number(x));
				});

				auto str = ss.str();
				if (not evaluates_to(exp, str))
					quit = true;
			};
		};
	};
};
