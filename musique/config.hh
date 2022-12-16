#ifndef MUSIQUE_CONFIG_HH
#define MUSIQUE_CONFIG_HH

#include <optional>
#include <string>
#include <unordered_map>

// Parses INI files
namespace config
{
	using Key_Value = std::unordered_map<std::string, std::string>;
	using Sections  = std::unordered_map<std::string, Key_Value>;

	Sections from_file(std::string const& path);
	void to_file(std::string const& path, Sections const& sections);

	std::string location();
}

#endif // MUSIQUE_CONFIG_HH
