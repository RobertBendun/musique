#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;

static void expect_token_type(
		Token::Type expected_type,
		std::string source,
		reflection::source_location const& sl = reflection::source_location::current())
{
	Lexer lexer{source};
	auto result = lexer.next_token();
	expect(result.has_value() >> fatal, sl) << "have not parsed any tokens";
	expect(eq(result->type, expected_type), sl) << "different token type then expected";
}

static void expect_token_type_and_value(
		Token::Type expected_type,
		std::string_view source,
		std::string_view expected,
		reflection::source_location const& sl = reflection::source_location::current())
{
	Lexer lexer{source};
	auto result = lexer.next_token();
	expect(result.has_value(), sl) << "have not parsed any tokens";

	if (result.has_value()) {
		expect(eq(result->type, expected_type), sl) << "different token type then expected";
		expect(eq(result->source, expected), sl) << "tokenized source is not equal to original";
	}
}

static void expect_token_type_and_value(
		Token::Type expected_type,
		std::string_view source,
		reflection::source_location const& sl = reflection::source_location::current())
{
	expect_token_type_and_value(expected_type, source, source, sl);
}

static void expect_token_type_and_location(
		Token::Type expected_type,
		std::string_view source,
		Location location,
		reflection::source_location const& sl = reflection::source_location::current())
{
	Lexer lexer{source};
	auto result = lexer.next_token();
	expect(result.has_value() >> fatal, sl) << "have not parsed any tokens";
	expect(eq(result->type, expected_type), sl) << "different token type then expected";
	expect(eq(result->location, location), sl) << "tokenized source is at different place then expected";
}

static void expect_empty_file(
		std::string_view source,
		reflection::source_location const& sl = reflection::source_location::current())
{
		Lexer lexer{source};
		auto result = lexer.next_token();
		expect(!result.has_value(), sl) << "could not produce any tokens from empty file";
		if (not result.has_value()) {
			expect(result.error() == errors::End_Of_File, sl) << "could not produce any tokens from empty file";
		}
}

template<auto N>
static void expect_token_sequence(
		std::string_view source,
		std::array<Token, N> const& expected_tokens,
		reflection::source_location const& sl = reflection::source_location::current())
{
	Lexer lexer{source};

	for (Token const& expected : expected_tokens) {
		auto const result = lexer.next_token();
		expect(result.has_value(), sl)                  << "expected token, received nothing";

		if (result.has_value()) {
			expect(eq(result->type, expected.type))         << "different token type then expected";
			expect(eq(result->source, expected.source))     << "different token source then expected";
			expect(eq(result->location, expected.location)) << "different token location then expected";
		}
	}

	auto const result = lexer.next_token();
	expect(not result.has_value(), sl) << "more tokens then expected";
}

suite lexer_test = [] {
	"Empty file"_test = [] {
		expect_empty_file("");
	};

	"Comments"_test = [] {
		expect_empty_file("#!/bin/sh");
		expect_empty_file("-- line comment");
		expect_token_type_and_value(Token::Type::Numeric, "--- block comment --- 0", "0");
		expect_empty_file(R"musique(
		--- hello
		multiline comment
		---
		)musique");
	};

	"Simple token types"_test = [] {
		expect_token_type(Token::Type::Close_Block,          "]");
		expect_token_type(Token::Type::Close_Paren,          ")");
		expect_token_type(Token::Type::Open_Block,           "[");
		expect_token_type(Token::Type::Open_Paren,           "(");
		expect_token_type(Token::Type::Variable_Separator,   "|");
		expect_token_type(Token::Type::Expression_Separator, ";");
	};

	"Numeric tokens"_test = [] {
		expect_token_type_and_value(Token::Type::Numeric, "0");
		expect_token_type_and_value(Token::Type::Numeric, "123456789");
		expect_token_type_and_value(Token::Type::Numeric, ".75");
		expect_token_type_and_value(Token::Type::Numeric, "0.75");
		expect_token_type_and_value(Token::Type::Numeric, "123456789.123456789");
		expect_token_type_and_value(Token::Type::Numeric, "123.", "123");
		expect_token_type_and_value(Token::Type::Numeric, " 1   ", "1");
		expect_token_type_and_value(Token::Type::Numeric, " 123   ", "123");
	};

	"Proper location marking"_test = [] {
		expect_token_type_and_location(Token::Type::Numeric, "123", Location::at(1, 1));
		expect_token_type_and_location(Token::Type::Numeric, "   123", Location::at(1, 4));
		expect_token_type_and_location(Token::Type::Numeric, "\n123", Location::at(2, 1));
		expect_token_type_and_location(Token::Type::Numeric, "\n  123", Location::at(2, 3));
	};

	"Chord literals"_test = [] {
		expect_token_type_and_value(Token::Type::Chord, "c");
		expect_token_type_and_value(Token::Type::Chord, "c#");
		expect_token_type_and_value(Token::Type::Chord, "c1");
		expect_token_type_and_value(Token::Type::Chord, "d1257");
		expect_token_type_and_value(Token::Type::Chord, "e#5'");
		expect_token_type_and_value(Token::Type::Chord, "g127");
		expect_token_type_and_value(Token::Type::Chord, "f1'2'3'5'7'");
		expect_token_type_and_value(Token::Type::Chord, "b1,2,5,7,");
	};

	"Symbol literals"_test = [] {
		expect_token_type_and_value(Token::Type::Symbol, "i");
		expect_token_type_and_value(Token::Type::Symbol, "i2");
		expect_token_type_and_value(Token::Type::Symbol, "example");
		expect_token_type_and_value(Token::Type::Symbol, "d1envelope");
		expect_token_type_and_value(Token::Type::Symbol, "snake_case");
		expect_token_type_and_value(Token::Type::Symbol, "camelCase");
		expect_token_type_and_value(Token::Type::Symbol, "PascalCase");
		expect_token_type_and_value(Token::Type::Symbol, "haskell'");
		expect_token_type_and_value(Token::Type::Symbol, "zażółć");
		expect_token_type_and_value(Token::Type::Symbol, "$foo");
		expect_token_type_and_value(Token::Type::Symbol, "@bar");
	};

	"Operators"_test = [] {
		expect_token_type_and_value(Token::Type::Operator, "+");
		expect_token_type_and_value(Token::Type::Operator, "&&");
		expect_token_type_and_value(Token::Type::Operator, "||");
		expect_token_type_and_value(Token::Type::Operator, "*");
		expect_token_type_and_value(Token::Type::Operator, "**");
		expect_token_type_and_value(Token::Type::Operator, "=");
		expect_token_type_and_value(Token::Type::Operator, "<");
		expect_token_type_and_value(Token::Type::Operator, ":");
		expect_token_type_and_value(Token::Type::Operator, "v");
		expect_token_type_and_value(Token::Type::Operator, "%");
	};

	"Multiple tokens"_test = [] {
		Location l;

		expect_token_sequence("1 + foo", std::array {
			Token { Token::Type::Numeric,  "1",   l.at(1, 1) },
			Token { Token::Type::Operator, "+",   l.at(1, 3) },
			Token { Token::Type::Symbol,   "foo", l.at(1, 5) }
		});

		expect_token_sequence("foo 1 2; bar 3 4", std::array {
			Token { Token::Type::Symbol,               "foo", l.at(1,  1) },
			Token { Token::Type::Numeric,              "1",   l.at(1,  5) },
			Token { Token::Type::Numeric,              "2",   l.at(1,  7) },
			Token { Token::Type::Expression_Separator, ";",   l.at(1,  8) },
			Token { Token::Type::Symbol,               "bar", l.at(1, 10) },
			Token { Token::Type::Numeric,              "3",   l.at(1, 14) },
			Token { Token::Type::Numeric,              "4",   l.at(1, 16) }
		});
	};
};
