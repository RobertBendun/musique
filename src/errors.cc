#include <musique.hh>

#include <iostream>
#include <sstream>
#include <stdexcept>

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

/// Describes type of error
enum class Error_Level
{
	Error,
	Bug
};

/// Centralized production of error headings for consistent error style
///
/// @param short_description General description of an error that will be printed in the heading
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

/// Prints message that should encourage contact with developers
static void encourage_contact(std::ostream &os)
{
	os << pretty::begin_comment << "\n"
		"Interpreter got in state that was not expected by it's developers.\n"
		"Contact them and provide code that coused this error and\n"
		"error message above to resolve this trouble\n"
		<< pretty::end << std::flush;
}

void assert(bool condition, std::string message, Location loc)
{
	if (condition) return;
#if Debug
	std::cout << "assertion failed at " << loc << " with message: " << message << std::endl;
	throw std::runtime_error(message);
#else
	error_heading(std::cerr, loc, Error_Level::Bug, "Assertion in interpreter");

	if (not message.empty())
		std::cerr << message  << std::endl;

	encourage_contact(std::cerr);

	std::exit(42);
#endif
}

#if !Debug
[[noreturn]]
#endif
void unimplemented(std::string_view message, Location loc)
{
#if Debug
	std::cout << "unreachable state reached at " << loc << " with message: " << message << std::endl;
	throw std::runtime_error(std::string(message));
#else
	error_heading(std::cerr, loc, Error_Level::Bug, "This part of interpreter was not implemented yet");

	if (not message.empty()) {
		std::cerr << message << std::endl;
	}

	encourage_contact(std::cerr);

	std::exit(42);
#endif
}

#if !Debug
[[noreturn]]
#endif
void unreachable(Location loc)
{
#if Debug
	std::cout << "unreachable state reached at " << loc << std::endl;
	throw std::runtime_error("reached unreachable");
#else
	error_heading(std::cerr, loc, Error_Level::Bug, "Reached unreachable state");
	encourage_contact(std::cerr);
	std::exit(42);
#endif
}

std::ostream& operator<<(std::ostream& os, Error const& err)
{
	std::string_view short_description = visit(Overloaded {
		[](errors::Operation_Requires_Midi_Connection const&)   { return "Operation requires MIDI connection"; },
		[](errors::Missing_Variable const&)                     { return "Cannot find variable"; },
		[](errors::Failed_Numeric_Parsing const&)               { return "Failed to parse a number"; },
		[](errors::Not_Callable const&)                         { return "Value not callable"; },
		[](errors::Undefined_Operator const&)                   { return "Undefined operator"; },
		[](errors::Unexpected_Empty_Source const&)              { return "Unexpected end of file"; },
		[](errors::Unexpected_Keyword const&)                   { return "Unexpected keyword"; },
		[](errors::Unrecognized_Character const&)               { return "Unrecognized character"; },
		[](errors::internal::Unexpected_Token const&)           { return "Unexpected token"; },
		[](errors::Expected_Expression_Separator_Before const&) { return "Missing semicolon"; },
		[](errors::Literal_As_Identifier const&)                { return "Literal used in place of an identifier"; },
		[](errors::Unsupported_Types_For const& type)           {
			return type.type == errors::Unsupported_Types_For::Function
				? "Function called with wrong arguments"
				: "Operator does not support given values";
		},
	}, err.details);

	error_heading(os, err.location, Error_Level::Error, short_description);

	auto const loc = err.location;
	auto const print_error_line = [&] (std::optional<Location> loc) {
		if (loc) {
			Lines::the.print(os, std::string(loc->filename), loc->line, loc->line);
			os << '\n';
		}
	};

	visit(Overloaded {
		[&](errors::Operation_Requires_Midi_Connection const& err) {
			os << "I cannot '" << err.name << "' due to lack of MIDI " << (err.is_input ? "input" : "output") << "connection\n";
			os << "\n";

			print_error_line(loc);

			os << "You can connect to given MIDI device by specifing port when running musique command like:\n";
			os << (err.is_input ? "  --input" : "  --output") << " PORT\n";
			os << "You can list all available ports with --list flag\n";
		},
		[&](errors::Missing_Variable const& err) {
			os << "I encountered '" << err.name << "' that looks like variable but\n";
			os << "I can't find it in surrounding scope or in one of parent's scopes\n";
			os << "\n";

			print_error_line(loc);

			os << "Variables can only be references in scope (block) where they been created\n";
			os << "or from parent blocks to variable block\n";
		},
		[&](errors::Unrecognized_Character const& err) {
			os << "I encountered character in the source code that was not supposed to be here.\n";
			os << "  Character Unicode code: U+" << std::hex << err.invalid_character << '\n';
			os << "  Character printed: '" << utf8::Print{err.invalid_character} << "'\n";
			os << "\n";

			print_error_line(loc);

			os << "Musique only accepts characters that are unicode letters or ascii numbers and punctuation\n";
		},

		[&](errors::Failed_Numeric_Parsing const& err) {
			constexpr auto Max = std::numeric_limits<decltype(Number::num)>::max();
			constexpr auto Min = std::numeric_limits<decltype(Number::num)>::min();
			os << "I tried to parse numeric literal, but I failed.\n\n";

			print_error_line(loc);

			if (err.reason == std::errc::result_out_of_range) {
				os << "Declared number is outside of valid range of numbers that can be represented.\n";
				os << "Only numbers in range [" << Min << ", " << Max << "] are supported\n";
			}
		},

		[&](errors::internal::Unexpected_Token const& ut) {
			os << "I encountered unexpected token during " << ut.when << '\n';
			os << "  Token type: " << ut.type << '\n';
			os << "  Token source: " << ut.source << "\n\n";

			print_error_line(loc);

			os << pretty::begin_comment << "This error is considered an internal one. It should not be displayed to the end user.\n";
			os << "\n";
			os << "This error message is temporary and will be replaced by better one in the future\n";
			os << pretty::end;
		},

		[&](errors::Expected_Expression_Separator_Before const& err) {
			os << "I failed to parse following code, due to missing semicolon before it!\n\n";

			print_error_line(loc);

			if (err.what == "var") {
				os << "If you want to create variable inside expression try wrapping them inside parentheses like this:\n";
				os << "    10 + (var i = 20)\n";
			}
		},

		[&](errors::Literal_As_Identifier const& err) {
			os << "I expected an identifier in " << err.context << ", but found" << (err.type_name.empty() ? "" : " ") << err.type_name << " value = '" << err.source << "'\n\n";

			print_error_line(loc);

			if (err.type_name == "chord") {
				os << "Try renaming to different name or appending with something that is not part of chord literal like 'x'\n";

				os << pretty::begin_comment <<
					"\nMusical notation names are reserved for chord and note notations,\n"
					"and cannot be reused as an identifier to prevent ambiguity\n"
					<< pretty::end;
			}
		},

		[&](errors::Unsupported_Types_For const& err) {
			switch (err.type) {
			case errors::Unsupported_Types_For::Function:
				{
					os << "I tried to call function '" << err.name << "' but you gave me wrong types for it!\n";

					print_error_line(loc);

					os << "Make sure that all values matches one of supported signatures listed below!\n";
					os << '\n';

					for (auto const& possibility : err.possibilities) {
						os << "  " << possibility << '\n';
					}
				}
				break;
			case errors::Unsupported_Types_For::Operator:
				{
					os << "I tried and failed to evaluate operator '" << err.name << "' due to values with wrong types provided\n";
					os << "Make sure that both values matches one of supported signatures listed below!\n";
					os << '\n';
					print_error_line(loc);


					if (err.name == "+") { os << "Addition only supports:\n"; }
					else { os << "Operator '" << err.name << "' only supports:\n"; }

					for (auto const& possibility : err.possibilities) {
						os << "  " << possibility << '\n';
					}
				}
				break;
			}
		},

		[&](errors::Not_Callable const& err) {
			os << "Value of type " << err.type << " cannot be called.\n";
			os << "\n";

			print_error_line(loc);

			os << "Only values of this types can be called:\n";
			os << "  - musical values like c, c47, (c&g) can be called to provide octave and duration\n";
			os << "  - blocks can be called to compute their body like this: [i | i + 1] 3\n";
			os << "  - builtin functions like if, par, floor\n";
			os << "\n";

			os << pretty::begin_comment;
			os << "Parhaps you forgot to include semicolon between successive elements?\n";
			os << "Common problem is writing [4 3] as list with elements 4 and 3,\n";
			os << "when correct code for such expression would be [4;3]\n";
			os << pretty::end;
		},
		[&](errors::Unexpected_Empty_Source const& err) {
			switch (err.reason) {
			break; case errors::Unexpected_Empty_Source::Block_Without_Closing_Bracket:
				os << "Reached end of input when waiting for closing of a block ']'\n";
				os << "Expected block end after:\n\n";
				print_error_line(loc);

				if (err.start) {
					os << "Which was introduced by block starting here:\n\n";
					print_error_line(err.start);
				}
			}
		},
		[&](errors::Undefined_Operator const& err) {
			os << "I encountered '" << err.op <<  "' that looks like operator but it isn't one.\n";
			os << '\n';

			print_error_line(loc);
		},
		[&](errors::Unexpected_Keyword const&)               { unimplemented(); },
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
