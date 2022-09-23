#ifndef MUSIQUE_LINES_HH
#define MUSIQUE_LINES_HH

#include <vector>
#include <string_view>
#include <unordered_map>

struct Lines
{
	static Lines the;

	/// Region of lines in files
	std::unordered_map<std::string, std::vector<std::string_view>> lines;

	/// Add lines from file
	void add_file(std::string filename, std::string_view source);

	/// Add single line into file (REPL usage)
	void add_line(std::string const& filename, std::string_view source, unsigned line_number);

	/// Print selected region
	void print(std::ostream& os, std::string const& file, unsigned first_line, unsigned last_line) const;
};

#endif
