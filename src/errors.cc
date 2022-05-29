#include <musique.hh>

#include <iostream>
#include <sstream>

template<typename T, typename ...Params>
concept Callable = requires(T t, Params ...params) { t(params...); };

template<typename Lambda, typename ...T>
concept Visitor = (Callable<Lambda, T> && ...);

/// Custom visit function for better C++ compilation error messages
template<typename V, typename ...T>
static auto visit(V &&visitor, std::variant<T...> const& variant)
{
	static_assert(Visitor<V, T...>, "visitor must cover all types");
	if constexpr (Visitor<V, T...>) {
		return std::visit(std::forward<V>(visitor), variant);
	}
}

Error Error::with(Location loc) &&
{
	location = loc;
	return *this;
}

enum class Error_Level
{
	Error,
	Bug
};

static std::ostream& error_heading(
		std::ostream &os,
		std::optional<Location> location,
		Error_Level lvl,
		std::string_view short_description)
{
	switch (lvl) {
	case Error_Level::Error:
		os << "ERROR " << short_description << ' ';
		break;

	// This branch should be reached if we have Error_Level::Bug
	// or definetely where error level is outside of enum Error_Level
	default:
		os << "IMPLEMENTATION BUG " << short_description << ' ';
	}

	if (location) {
		os << " at " << *location;
	}

	return os << '\n';
}

static void encourage_contact(std::ostream &os)
{
	os <<
		"Interpreter got in state that was not expected by it's developers.\n"
		"Contact them providing code that coused it and error message above to resolve this trouble\n"
		<< std::flush;
}

void assert(bool condition, std::string message, Location loc)
{
	if (condition) return;
	error_heading(std::cerr, loc, Error_Level::Bug, "Assertion in interpreter");

	if (not message.empty())
		std::cerr << "with message: " << message << std::endl;

	encourage_contact(std::cerr);

	std::exit(1);
}

[[noreturn]] void unimplemented(std::string_view message, Location loc)
{
	error_heading(std::cerr, loc, Error_Level::Bug, "This part of interpreter was not implemented yet");

	if (not message.empty()) {
		std::cerr << message << std::endl;
	}

	encourage_contact(std::cerr);

	std::exit(1);
}

[[noreturn]] void unreachable(Location loc)
{

	error_heading(std::cerr, loc, Error_Level::Bug, "Reached unreachable state");
	encourage_contact(std::cerr);
	std::exit(1);
}

std::ostream& operator<<(std::ostream& os, Error const& err)
{
	std::string_view short_description = visit(Overloaded {
		[](errors::Expected_Keyword const&)                 { return "Expected keyword"; },
		[](errors::Failed_Numeric_Parsing const&)           { return "Failed to parse a number"; },
		[](errors::Music_Literal_Used_As_Identifier const&) { return "Music literal in place of identifier"; },
		[](errors::Not_Callable const&)                     { return "Value not callable"; },
		[](errors::Undefined_Identifier const&)             { return "Undefined identifier"; },
		[](errors::Undefined_Operator const&)               { return "Undefined operator"; },
		[](errors::Unexpected_Empty_Source const&)          { return "Unexpected end of file"; },
		[](errors::Unexpected_Keyword const&)               { return "Unexpected keyword"; },
		[](errors::Unrecognized_Character const&)           { return "Unrecognized character"; },
		[](errors::internal::Unexpected_Token const&)       { return "Unexpected token"; }
	}, err.details);

	error_heading(os, err.location, Error_Level::Error, short_description);

	visit(Overloaded {
		[&os](errors::Unrecognized_Character const& err) {
			os << "I encountered character in the source code that was not supposed to be here.\n";
			os << "  Character Unicode code: " << std::hex << err.invalid_character << '\n';
			os << "  Character printed: '" << utf8::Print{err.invalid_character} << "'\n";
			os << "\n";
			os << "Musique only accepts characters that are unicode letters or ascii numbers and punctuation\n";
		},

		[&os](errors::Failed_Numeric_Parsing const& err) {
			constexpr auto Max = std::numeric_limits<decltype(Number::num)>::max();
			constexpr auto Min = std::numeric_limits<decltype(Number::num)>::min();
			os << "I tried to parse numeric literal, but I failed.";
			if (err.reason == std::errc::result_out_of_range) {
				os << " Declared number is outside of valid range of numbers that can be represented.\n";
				os << "  Only numbers in range [" << Min << ", " << Max << "] are supported\n";
			}
		},

		[&os](errors::internal::Unexpected_Token const& ut) {
			os << "I encountered unexpected token during " << ut.when << '\n';
			os << "  Token type: " << ut.type << '\n';
			os << "  Token source: " << ut.source << '\n';

			os << "\nThis error is considered an internal one. It should not be displayed to the end user.\n";
			encourage_contact(os);
		},

		[&os](errors::Expected_Keyword const&)                 {},
		[&os](errors::Music_Literal_Used_As_Identifier const&) {},
		[&os](errors::Not_Callable const&)                     {},
		[&os](errors::Undefined_Identifier const&)             {},
		[&os](errors::Undefined_Operator const&)               {},
		[&os](errors::Unexpected_Keyword const&)               {},
		[&os](errors::Unexpected_Empty_Source const&)          {}
	}, err.details);

	return os;
}

void errors::all_tokens_were_not_parsed(std::span<Token> tokens)
{
	error_heading(std::cerr, std::nullopt, Error_Level::Bug, "Remaining tokens");
	std::cerr << "remaining tokens after parsing. Listing remaining tokens:\n";

	for (auto const& token : tokens) {
		std::cerr << token << '\n';
	}

	std::exit(1);
}
