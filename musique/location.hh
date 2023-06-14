#ifndef MUSIQUE_LOCATION_HH
#define MUSIQUE_LOCATION_HH

#if __has_include(<source_location>)
#include <source_location>
#endif

#include <musique/common.hh>
#include <ostream>

struct File_Range
{
	std::string_view filename{};
	unsigned start = 0;
	unsigned stop  = 0;

	explicit inline operator bool() const { return filename.size(); }

	File_Range operator+(File_Range const&) const;
	File_Range& operator+=(File_Range const&);

	bool operator==(File_Range const&) const = default;
};

/// \brief Location describes code position in `file line column` format.
///        It's used both to represent position in source files provided
//         to interpreter and internal interpreter usage.
struct Location
{
	std::string_view filename = "<unnamed>"; ///< File that location is pointing to
	usize line   = 1;                        ///< Line number (1 based) that location is pointing to
	usize column = 1;                        ///< Column number (1 based) that location is pointing to
	std::string_view function_name = "";     ///< Name of the function where location is pointing to

	/// Advances line and column numbers based on provided rune
	///
	/// If rune is newline, then column is reset to 1, and line number is incremented.
	/// Otherwise column number is incremented.
	///
	/// @param rune Rune from which column and line numbers advancements are made.
	Location& advance(u32 rune);

	bool operator==(Location const& rhs) const = default;

	//! Creates location at default filename with specified line and column number
	static Location at(usize line, usize column);

	// Used to describe location of function call in interpreter (internal use only)
#if defined(__cpp_lib_source_location)
	static Location caller(std::source_location loc = std::source_location::current());
#elif (__has_builtin(__builtin_FILE) and __has_builtin(__builtin_LINE))
	static Location caller(char const* file = __builtin_FILE(), usize line = __builtin_LINE());
#else
#error Cannot implement Location::caller function
	/// Returns location of call in interpreter source code.
	///
	/// Example of reporting where `foo()` was beeing called:
	/// @code
	/// void foo(Location loc = Location::caller()) { std::cout << loc << '\n'; }
	/// @endcode
	static Location caller();
#endif
};

std::ostream& operator<<(std::ostream& os, Location const& location);

#endif
