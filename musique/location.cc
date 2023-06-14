#include <musique/location.hh>

File_Range File_Range::operator+(File_Range const& rhs) const
{
	return {
		.filename = filename.empty() ? rhs.filename : filename,
		.start = std::min(start, rhs.start),
		.stop = std::max(stop, rhs.stop),
	};
}

File_Range& File_Range::operator+=(File_Range const& rhs)
{
	filename = filename.empty() ? rhs.filename : filename;
	start = std::min(start, rhs.start);
	stop = std::max(stop, rhs.stop);
	return *this;
}

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
	os << location.filename << ':' << location.line << ':' << location.column;
	if (location.function_name.size()) {
		os << ":" <<  location.function_name;
	}
	return os;
}

#if defined(__cpp_lib_source_location)
Location Location::caller(std::source_location loc)
{
	return Location { loc.file_name(), loc.line(), loc.column(), loc.function_name() };
}
#elif (__has_builtin(__builtin_FILE) and __has_builtin(__builtin_LINE))
Location Location::caller(char const* file, usize line)
{
	return Location { file, line };
}
#endif
