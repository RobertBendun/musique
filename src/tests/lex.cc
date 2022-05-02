#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;

auto under(auto enumeration) requires std::is_enum_v<decltype(enumeration)>
{
	return static_cast<std::underlying_type_t<decltype(enumeration)>>(enumeration);
}

static void expect_token_type(
		Token::Type expected_type,
		std::string source,
		reflection::source_location const& sl = reflection::source_location::current())
{
	Lexer lexer{source};
	auto result = lexer.next_token();
	expect(result.has_value() >> fatal, sl) << "have not parsed any tokens";
	expect(eq(under(result->type), under(expected_type)), sl) << "different token type then expected";
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
		expect(eq(under(result->type), under(expected_type)), sl) << "different token type then expected";
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
	expect(eq(under(result->type), under(expected_type)), sl) << "different token type then expected";
	expect(eq(result->location, location), sl) << "tokenized source is at different place then expected";
}

suite lexer_test = [] {
	"Empty file"_test = [] {
		Lexer lexer{""};
		auto result = lexer.next_token();
		expect(!result.has_value() >> fatal) << "could not produce any tokens from empty file";
		expect(result.error() == errors::End_Of_File) << "could not produce any tokens from empty file";
	};

	"Simple token types"_test = [] {
		expect_token_type(Token::Type::Close_Block,        "]");
		expect_token_type(Token::Type::Close_Paren,        ")");
		expect_token_type(Token::Type::Open_Block,         "[");
		expect_token_type(Token::Type::Open_Paren,         "(");
		expect_token_type(Token::Type::Variable_Separator, "|");
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
		expect_token_type_and_value(Token::Type::Chord, "c1");
		expect_token_type_and_value(Token::Type::Chord, "d1257");
		expect_token_type_and_value(Token::Type::Chord, "e1'");
		expect_token_type_and_value(Token::Type::Chord, "g127");
		expect_token_type_and_value(Token::Type::Chord, "f1'2'3'5'7'");
		expect_token_type_and_value(Token::Type::Chord, "b1,2,5,7,");
	};

	"Symbol literals"_test = [] {
		expect_token_type_and_value(Token::Type::Symbol, "i");
		expect_token_type_and_value(Token::Type::Symbol, "i2");
		expect_token_type_and_value(Token::Type::Symbol, "example");
		expect_token_type_and_value(Token::Type::Symbol, "d1envelope");
		expect_token_type_and_value(Token::Type::Symbol, "kebab-case");
		expect_token_type_and_value(Token::Type::Symbol, "snake_case");
		expect_token_type_and_value(Token::Type::Symbol, "camelCase");
		expect_token_type_and_value(Token::Type::Symbol, "PascalCase");
		expect_token_type_and_value(Token::Type::Symbol, "haskell'");
	};
};
