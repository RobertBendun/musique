#ifndef MUSIQUE_PRETTY_HH
#define MUSIQUE_PRETTY_HH

#include <ostream>

/// All code related to pretty printing. Default mode is no_color
namespace pretty
{
	/// Mark start of printing an error
	std::ostream& begin_error(std::ostream&);

	/// Mark start of printing a path
	std::ostream& begin_path(std::ostream&);

	/// Mark start of printing a comment
	std::ostream& begin_comment(std::ostream&);

	/// Mark end of any above
	std::ostream& end(std::ostream&);

	/// Switch to colorful output via ANSI escape sequences
	void terminal_mode();

	/// Switch to colorless output (default one)
	void no_color_mode();
}

#endif
