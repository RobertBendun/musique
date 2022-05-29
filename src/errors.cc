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
	std::stringstream ss;

	switch (lvl) {
	case Error_Level::Error:
		ss << pretty::begin_error << "ERROR " << pretty::end << short_description;
		break;

	// This branch should be reached if we have Error_Level::Bug
	// or definetely where error level is outside of enum Error_Level
	default:
		ss << pretty::begin_error << "IMPLEMENTATION BUG " << pretty::end << short_description;
	}

	if (location) {
		ss << " at " << pretty::begin_path << *location << pretty::end;
	}

	auto heading = std::move(ss).str();
	os << heading << '\n';

	auto const line_length = heading.size();
	std::fill_n(std::ostreambuf_iterator<char>(os), line_length, '-');
	return os << '\n';
}

static void encourage_contact(std::ostream &os)
{
	os << pretty::begin_comment << "\n"
		"Interpreter got in state that was not expected by it's developers.\n"
		"Contact them providing code that coused it and error message above to resolve this trouble\n"
		<< pretty::end << std::flush;
}

void assert(bool condition, std::string message, Location loc)
{
	if (condition) return;
	error_heading(std::cerr, loc, Error_Level::Bug, "Assertion in interpreter");

	if (not message.empty())
		std::cerr << message  << std::endl;

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
		[](errors::Failed_Numeric_Parsing const&)               { return "Failed to parse a number"; },
		[](errors::Not_Callable const&)                         { return "Value not callable"; },
		[](errors::Undefined_Operator const&)                   { return "Undefined operator"; },
		[](errors::Unexpected_Empty_Source const&)              { return "Unexpected end of file"; },
		[](errors::Unexpected_Keyword const&)                   { return "Unexpected keyword"; },
		[](errors::Unrecognized_Character const&)               { return "Unrecognized character"; },
		[](errors::internal::Unexpected_Token const&)           { return "Unexpected token"; },
		[](errors::Expected_Expression_Separator_Before const&) { return "Missing semicolon"; }
	}, err.details);

	error_heading(os, err.location, Error_Level::Error, short_description);

	visit(Overloaded {
		[&os](errors::Unrecognized_Character const& err) {
			os << "I encountered character in the source code that was not supposed to be here.\n";
			os << "  Character Unicode code: U+" << std::hex << err.invalid_character << '\n';
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

			os << pretty::begin_comment << "\nThis error is considered an internal one. It should not be displayed to the end user.\n" << pretty::end;
			encourage_contact(os);
		},

		[&os](errors::Expected_Expression_Separator_Before const& err) {
			os << "I failed to parse following code, due to missing semicolon before it!\n";

			if (err.what == "var") {
				os << "\nIf you want to create variable inside expression try wrapping them inside parentheses like this:\n";
				os << "    10 + (var i = 20)\n";
			}
		},

		[&os](errors::Not_Callable const&)                     { unimplemented(); },
		[&os](errors::Undefined_Operator const&)               { unimplemented(); },
		[&os](errors::Unexpected_Keyword const&)               { unimplemented(); },
		[&os](errors::Unexpected_Empty_Source const&)          { unimplemented(); }
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
