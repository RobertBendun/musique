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
	expect(result.has_value() >> fatal, sl) << "have not parsed any tokens";
	expect(eq(under(result->type), under(expected_type)), sl) << "different token type then expected";
	expect(eq(result->source, expected)) << "tokenized source is not equal to original";
}

static void expect_token_type_and_value(
		Token::Type expected_type,
		std::string_view source,
		reflection::source_location const& sl = reflection::source_location::current())
{
	expect_token_type_and_value(expected_type, source, source, sl);
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
	};
};
