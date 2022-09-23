#include <musique/location.hh>

Location Location::at(usize line, usize column)
{
	Location loc;
	loc.line = line;
	loc.column = column;
	return loc;
}

Location& Location::advance(u32 rune)
{
	switch (rune) {
	case '\n':
		line += 1;
		[[fallthrough]];
	case '\r':
		column = 1;
		return *this;
	}
	column += 1;
	return *this;
}

std::ostream& operator<<(std::ostream& os, Location const& location)
{
	return os << location.filename << ':' << location.line << ':' << location.column;
}

#if defined(__cpp_lib_source_location)
Location Location::caller(std::source_location loc = std::source_location::current())
{
	return Location { loc.file_name(), loc.line(), loc.column() };
}
#elif (__has_builtin(__builtin_FILE) and __has_builtin(__builtin_LINE))
Location Location::caller(char const* file, usize line)
{
	return Location { file, line };
}
#endif
