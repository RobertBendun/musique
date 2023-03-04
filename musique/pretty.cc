#include <musique/pretty.hh>

#ifdef _WIN32
extern "C" {
#include <windows.h>
}
#endif

namespace starters
{
	static std::string_view Bold;
	static std::string_view Comment;
	static std::string_view End;
	static std::string_view Error;
	static std::string_view Path;
}

std::ostream& pretty::begin_error(std::ostream& os)
{
	return os << starters::Error;
}

std::ostream& pretty::begin_path(std::ostream& os)
{
	return os << starters::Path;
}

std::ostream& pretty::begin_comment(std::ostream& os)
{
	return os << starters::Comment;
}

std::ostream& pretty::begin_bold(std::ostream& os)
{
	return os << starters::Bold;
}

std::ostream& pretty::end(std::ostream& os)
{
	return os << starters::End;
}

void pretty::terminal_mode()
{
	// Windows 10 default commandline window doesn't support ANSI escape codes
	// by default in terminal output. This feature can be enabled via
	// ENABLE_VIRTUAL_TERMINAL_INPUT flag. Windows Terminal shouldn't have this issue.
#ifdef _WIN32 // FIXME replace this preprocessor hack with if constexpr
	{
		auto standard_output = GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD mode;
		if (!GetConsoleMode(standard_output, &mode) ) {
			return;
		}
		if ((mode & ENABLE_VIRTUAL_TERMINAL_INPUT) != ENABLE_VIRTUAL_TERMINAL_INPUT) {
			mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
			if (!SetConsoleMode(standard_output, mode)) {
				return;
			}
		}
	}
#endif


	starters::Bold     = "\x1b[1m";
	starters::Comment  = "\x1b[30;1m";
	starters::End      = "\x1b[0m";
	starters::Error    = "\x1b[31;1m";
	starters::Path     = "\x1b[34;1m";
}

void pretty::no_color_mode()
{
	starters::Bold    = {};
	starters::Comment = {};
	starters::End     = {};
	starters::Error   = {};
	starters::Path    = {};
}
