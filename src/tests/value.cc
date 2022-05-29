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

static void test_note_resolution(
	std::string_view name,
	i8 octave,
	u8 expected,
	reflection::source_location sl = reflection::source_location::current())
{
	auto const maybe_note = Note::from(name);
	expect(maybe_note.has_value(), sl) << "Note::from didn't recognized " << name << " as a note";
	if (maybe_note) {
		auto note = *maybe_note;
		note.octave = octave;
		auto const midi_note = note.into_midi_note();
		expect(midi_note.has_value(), sl) << "Note::into_midi_note returned nullopt, but should not";
		expect(eq(int(*midi_note), int(expected)), sl) << "Note::into_midi_note returned wrong value";
	}
}

static void test_note_resolution(
	std::string_view name,
	u8 expected,
	reflection::source_location sl = reflection::source_location::current())
{
	auto const maybe_note = Note::from(name);
	expect(maybe_note.has_value(), sl) << "Note::from didn't recognized " << name << " as a note";
	if (maybe_note) {
		auto note = *maybe_note;
		note.octave = 4;
		auto const midi_note = note.into_midi_note();
		expect(midi_note.has_value(), sl) << "Note::into_midi_note returned nullopt, but should not";
		expect(eq(int(*midi_note), int(expected)), sl) << "Note::into_midi_note returned wrong value";
	}
}

suite value_test = [] {
	"Value"_test = [] {
		should("be properly created using Value::from") = [] {
			expect_value(Value::from({ Token::Type::Numeric, "10", {} }),     Value::from(Number(10)));
			expect_value(Value::from({ Token::Type::Keyword, "nil", {} }),    Value{});
			expect_value(Value::from({ Token::Type::Keyword, "true", {} }),   Value::from(true));
			expect_value(Value::from({ Token::Type::Keyword, "false", {} }),  Value::from(false));
			expect_value(Value::from({ Token::Type::Symbol,  "foobar", {} }), Value::from("foobar"));
		};

		should("have be considered truthy or falsy") = [] {
			either_truthy_or_falsy(&Value::truthy, Value::from(true));
			either_truthy_or_falsy(&Value::truthy, Value::from(Number(1)));
			either_truthy_or_falsy(&Value::truthy, Value::from("foo"));
			either_truthy_or_falsy(&Value::truthy, Value(Intrinsic(nullptr)));

			either_truthy_or_falsy(&Value::falsy, Value{});
			either_truthy_or_falsy(&Value::falsy, Value::from(false));
			either_truthy_or_falsy(&Value::falsy, Value::from(Number(0)));
		};
	};

	"Value comparisons"_test = [] {
		should("are always not equal when types differ") = [] {
			expect(neq(Value::from("0"), Value::from(Number(0))));
		};
	};

	"Value printing"_test = [] {
		expect(eq("nil"sv, str(Value{})));
		expect(eq("true"sv, str(Value::from(true))));
		expect(eq("false"sv, str(Value::from(false))));

		expect(eq("10"sv, str(Value::from(Number(10)))));
		expect(eq("1/2"sv, str(Value::from(Number(2, 4)))));

		expect(eq("foo"sv, str(Value::from("foo"))));

		expect(eq("<intrinsic>"sv, str(Value(Intrinsic(nullptr)))));
	};

	"Note"_test = [] {
		should("properly resolve notes") = [] {
			for (i8 i = 0; i < 9; ++i) {
				test_note_resolution("c",  i + -1, i * 12 +  0);
				test_note_resolution("c#", i + -1, i * 12 +  1);
				test_note_resolution("d",  i + -1, i * 12 +  2);
				test_note_resolution("d#", i + -1, i * 12 +  3);
				test_note_resolution("e",  i + -1, i * 12 +  4);
				test_note_resolution("e#", i + -1, i * 12 +  5);
				test_note_resolution("f",  i + -1, i * 12 +  5);
				test_note_resolution("f#", i + -1, i * 12 +  6);
				test_note_resolution("g",  i + -1, i * 12 +  7);
				test_note_resolution("g#", i + -1, i * 12 +  8);
				test_note_resolution("a",  i + -1, i * 12 +  9);
				test_note_resolution("a#", i + -1, i * 12 + 10);
				test_note_resolution("b",  i + -1, i * 12 + 11);
			}
		};

		should("Support flat and sharp") = [] {
			test_note_resolution("c#", 61);
			test_note_resolution("cs", 61);
			test_note_resolution("cf", 59);
			test_note_resolution("cb", 59);

			test_note_resolution("c##", 62);
			test_note_resolution("css", 62);
			test_note_resolution("cff", 58);
			test_note_resolution("cbb", 58);

			test_note_resolution("cs#", 62);
			test_note_resolution("c#s", 62);
			test_note_resolution("cbf", 58);
			test_note_resolution("cfb", 58);

			test_note_resolution("c#b", 60);
			test_note_resolution("cb#", 60);
			test_note_resolution("cfs", 60);
			test_note_resolution("csf", 60);

			test_note_resolution("c##########", 70);
			test_note_resolution("cbbbbbbbbbb", 50);
		};
	};
};
