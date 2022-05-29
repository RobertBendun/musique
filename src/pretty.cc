#include <musique.hh>

namespace starters
{
	static std::string_view Error;
	static std::string_view Path;
	static std::string_view Comment;
	static std::string_view End;
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

std::ostream& pretty::end(std::ostream& os)
{
	return os << starters::End;
}

void pretty::terminal_mode()
{
	starters::Error    = "\x1b[31;1m";
	starters::Path     = "\x1b[34;1m";
	starters::Comment  = "\x1b[30;1m";
	starters::End      = "\x1b[0m";
}

void pretty::no_color_mode()
{
	starters::Error   = {};
	starters::Path    = {};
	starters::Comment = {};
	starters::End     = {};
}
