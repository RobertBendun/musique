#include <musique.hh>

#include <iostream>
#include <sstream>

bool Error::operator==(errors::Type type)
{
	return this->type == type;
}

Error Error::with(Location loc) &&
{
	location = loc;
	return *this;
}

enum class Error_Level
{
	Error,
	Notice,
	Bug
};

static std::ostream& error_heading(std::ostream &os, std::optional<Location> location, Error_Level lvl)
{
	if (location) {
		os << *location;
	} else {
		os << "musique";
	}

	switch (lvl) {
	case Error_Level::Error:  return os << ": error: ";
	case Error_Level::Notice: return os << ": notice: ";

	// This branch should be reached if we have Error_Level::Bug
	// or definetely where error level is outside of enum Error_Level
	default:                  return os << ": implementation bug: ";
	}
}

void assert(bool condition, std::string message, Location loc)
{
	if (condition) return;
	error_heading(std::cerr, loc, Error_Level::Bug) << message << std::endl;
	std::exit(1);
}

[[noreturn]] void unimplemented(Location loc)
{
	error_heading(std::cerr, loc, Error_Level::Bug) << "this part was not implemented yet" << std::endl;
	std::exit(1);
}

[[noreturn]] void unreachable(Location loc)
{
	error_heading(std::cerr, loc, Error_Level::Bug) << "this place should not be reached" << std::endl;
	std::exit(1);
}

std::ostream& operator<<(std::ostream& os, Error const& err)
{
	error_heading(os, err.location, Error_Level::Error);

	switch (err.type) {
	case errors::End_Of_File:
		return os << "end of file\n";

	case errors::Unrecognized_Character:
		return err.message.empty() ? os << "unrecognized character\n" : os << err.message;

	case errors::Unexpected_Token_Type:
		return os << err.message;

	case errors::Unexpected_Empty_Source:
		return os << "unexpected end of input\n";

	case errors::Failed_Numeric_Parsing:
		return err.error_code == std::errc::result_out_of_range
			? os << "number " << err.message << " cannot be represented with " << (sizeof(Number::num)*8) << " bit number\n"
			: os << "couldn't parse number " << err.message << '\n';
	}

	return os << "unrecognized error type\n";
}

static std::string format(auto const& ...args)
{
	std::stringstream ss;
	(void) (ss << ... << args);
	return ss.str();
}

Error errors::unrecognized_character(u32 invalid_character)
{
	Error err;
	err.type = errors::Unrecognized_Character;
	err.message = format(
		"unrecognized charater U+",
		std::hex, invalid_character,
		" (char: '", utf8::Print{invalid_character}, "')");

	return err;
}

Error errors::unrecognized_character(u32 invalid_character, Location location)
{
	return unrecognized_character(invalid_character).with(std::move(location));
}

Error errors::unexpected_token(Token const& unexpected)
{
	Error err;
	err.type = errors::Unexpected_Token_Type;
	err.location = unexpected.location;
	err.message = format("unexpected ", unexpected.type);
	return err;
}

Error errors::unexpected_token(Token::Type expected, Token const& unexpected)
{
	Error err;
	err.type = errors::Unexpected_Token_Type;
	err.location = unexpected.location;
	err.message = format("expected ", expected, ", but got ", unexpected.type);
	return err;
}

Error errors::unexpected_end_of_source(Location location)
{
	Error err;
	err.type = errors::Unexpected_Empty_Source;
	err.location = location;
	return err;
}

Error errors::failed_numeric_parsing(Location location, std::errc errc, std::string_view source)
{
	Error err;
	err.type = errors::Failed_Numeric_Parsing;
	err.location = location;
	err.error_code = errc;
	err.message = source;
	return err;
}

void errors::all_tokens_were_not_parsed(std::span<Token> tokens)
{
	error_heading(std::cerr, std::nullopt, Error_Level::Bug);
	std::cerr << "remaining tokens after parsing. Listing remaining tokens:\n";

	for (auto const& token : tokens) {
		error_heading(std::cerr, token.location, Error_Level::Notice);
		std::cerr << token << '\n';
	}

	std::exit(1);
}
