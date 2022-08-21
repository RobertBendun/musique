#include <musique.hh>

#include <iomanip>

Lines Lines::the;

void Lines::add_file(std::string filename, std::string_view source)
{
	auto file = lines.insert({ filename, {} });
	auto &lines_in_file = file.first->second;

	while (not source.empty()) {
		auto end = source.find('\n');
		if (end == std::string_view::npos) {
			lines_in_file.push_back({ source.data(), end });
			break;
		} else {
			lines_in_file.push_back({ source.data(), end });
			source.remove_prefix(end+1);
		}
	}
}

void Lines::add_line(std::string const& filename, std::string_view source, unsigned line_number)
{
	assert(line_number != 0, "Line number = 0 is invalid");
	if (lines[filename].size() <= line_number)
		lines[filename].resize(line_number);
	lines[filename][line_number - 1] = source;
}

void Lines::print(std::ostream &os, std::string const& filename, unsigned first_line, unsigned last_line) const
{
	auto const& file = lines.at(filename);
	for (auto i = first_line; i <= last_line; ++i) {
		os << std::setw(3) << std::right << i << " | " << file[i-1] << '\n';
	}
	os << std::flush;
}
