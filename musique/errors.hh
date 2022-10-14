#ifndef MUSIQUE_ERRORS_HH
#define MUSIQUE_ERRORS_HH

#if defined(__cpp_lib_source_location)
#include <source_location>
#endif

#include <optional>
#include <system_error>
#include <variant>
#include <span>
#include <vector>
#include <numeric>

#include <musique/common.hh>
#include <musique/location.hh>

/// Guards that program exits if condition does not hold
void ensure(bool condition, std::string message, Location loc = Location::caller());

/// Marks part of code that was not implemented yet
[[noreturn]] void unimplemented(std::string_view message = {}, Location loc = Location::caller());

/// Marks location that should not be reached
[[noreturn]] void unreachable(Location loc = Location::caller());

/// Error handling related functions and definitions
namespace errors
{
	/// When user puts emoji in the source code
	struct Unrecognized_Character
	{
		u32 invalid_character;
	};

	/// When parser was expecting code but encountered end of file
	struct Unexpected_Empty_Source
	{
		enum { Block_Without_Closing_Bracket } reason;
		std::optional<Location> start;
	};

	/// When user passed numeric literal too big for numeric type
	struct Failed_Numeric_Parsing
	{
		std::errc reason;
	};

	/// When user forgot semicolon or brackets
	struct Expected_Expression_Separator_Before
	{
		std::string_view what;
	};

	/// When some keywords are not allowed in given context
	struct Unexpected_Keyword
	{
		std::string_view keyword;
	};

	/// When user tried to use operator that was not defined
	struct Undefined_Operator
	{
		std::string_view op;
	};

	/// When user tries to use operator with wrong arity of arguments
	struct Wrong_Arity_Of
	{
		/// Type of operation
		enum Type { Operator, Function } type;

		/// Name of operation
		std::string_view name;

		/// Arity that was expected by given operation
		size_t expected_arity;

		/// Arit that user provided
		size_t actual_arity;
	};

	/// When user tried to call something that can't be called
	struct Not_Callable
	{
		std::string_view type;
	};

	/// When user provides literal where identifier should be
	struct Literal_As_Identifier
	{
		std::string_view type_name;
		std::string_view source;
		std::string_view context;
	};

	/// When user provides wrong type for given operation
	struct Unsupported_Types_For
	{
		/// Type of operation
		enum Type { Operator, Function } type;

		/// Name of operation
		std::string_view name;

		/// Possible ways to use it correctly
		std::vector<std::string> possibilities;
	};

	/// When user tries to use variable that has not been defined yet.
	struct Missing_Variable
	{
		/// Name of variable
		std::string name;
	};

	/// When user tries to invoke some MIDI action but haven't established MIDI connection
	struct Operation_Requires_Midi_Connection
	{
		/// If its input or output connection missing
		bool is_input;

		/// Name of the operation that was beeing invoked
		std::string name;
	};

	/// When user tries to get element from collection with index higher then collection size
	struct Out_Of_Range
	{
		/// Index that was required by the user
		size_t required_index;

		/// Size of accessed collection
		size_t size;
	};

	struct Closing_Token_Without_Opening
	{
		enum {
			Block = ']',
			Paren = ')'
		} type;
	};

	struct Arithmetic
	{
		enum Type
		{
			Division_By_Zero,
			Fractional_Modulo,
			Unable_To_Calculate_Modular_Multiplicative_Inverse
		} type;
	};

	/// Collection of messages that are considered internal and should not be printed to the end user.
	namespace internal
	{
		/// When encountered token that was supposed to be matched in higher branch of the parser
		struct Unexpected_Token
		{
			/// Type of the token
			std::string_view type;

			/// Source of the token
			std::string_view source;

			/// Where this token was encountered that was unexpected?
			std::string_view when;
		};
	}

	/// All possible error types
	using Details = std::variant<
		Arithmetic,
		Closing_Token_Without_Opening,
		Expected_Expression_Separator_Before,
		Failed_Numeric_Parsing,
		Literal_As_Identifier,
		Missing_Variable,
		Not_Callable,
		Operation_Requires_Midi_Connection,
		Out_Of_Range,
		Undefined_Operator,
		Unexpected_Empty_Source,
		Unexpected_Keyword,
		Unrecognized_Character,
		Unsupported_Types_For,
		Wrong_Arity_Of,
		internal::Unexpected_Token
	>;
}

/// Represents all recoverable error messages that interpreter can produce
struct Error
{
	/// Specific message details
	errors::Details details;

	/// Location that coused all this trouble
	std::optional<Location> location = std::nullopt;

	/// Return self with new location
	Error with(Location) &&;
};

/// Error pretty printing
std::ostream& operator<<(std::ostream& os, Error const& err);

struct Token;

namespace errors
{
	[[noreturn]]
	void all_tokens_were_not_parsed(std::span<Token>);
}

#endif // MUSIQUE_ERRORS_HH
