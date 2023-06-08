#ifndef MUSIQUE_RUNNER_HH
#define MUSIQUE_RUNNER_HH

#include <cstdint>
#include <musique/bit_field.hh>
#include <musique/interpreter/interpreter.hh>

/// Execution_Options is set of flags controlling how Runner executes provided code
enum class Execution_Options : std::uint32_t
{
	Print_Ast_Only    = 1 << 0,
	Print_Result      = 1 << 1,
	Time_Execution    = 1 << 2,
	Print_Tokens_Only = 1 << 3,
};

template<>
struct Enable_Bit_Field_Operations<Execution_Options> : std::true_type
{
};

/// Runs interpreter on given source code
struct Runner
{
	static inline Runner *the;

	Interpreter interpreter;
	Execution_Options default_options = static_cast<Execution_Options>(0);

	/// Setup interpreter and midi connection with given port
	Runner();

	Runner(Runner const&) = delete;
	Runner(Runner &&) = delete;
	Runner& operator=(Runner const&) = delete;
	Runner& operator=(Runner &&) = delete;

	/// Consider given input file as new definition of parametless function
	///
	/// Useful for deffering execution of files to the point when all configuration of midi devices
	/// is beeing known as working.
	std::optional<Error> deffered_file(std::string_view source, std::string_view filename);
	/// Run given source
	std::optional<Error> run(std::string_view source, std::string_view filename, Execution_Options flags = static_cast<Execution_Options>(0));
};


#endif // MUSIQUE_RUNNER_HH
