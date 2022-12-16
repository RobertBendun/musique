#include <fstream>
#include <iterator>
#include <musique/config.hh>
#include <musique/errors.hh>
#include <musique/platform.hh>

// FIXME Move trim to some header
extern void trim(std::string_view &s);

// FIXME Report errors when parsing fails
config::Sections config::from_file(std::string const& path)
{
	Sections sections;
	Key_Value *kv = &sections[""];

	std::ifstream in(path);
	for (std::string linebuf; std::getline(in, linebuf); ) {
		std::string_view line = linebuf;
		trim(line);

		if (auto comment = line.find('#'); comment != std::string_view::npos) {
			line = line.substr(0, comment);
		}

		if (line.starts_with('[') && line.ends_with(']')) {
			kv = &sections[std::string(line.begin()+1, line.end()-1)];
			continue;
		}

		if (auto split = line.find('='); split != std::string_view::npos) {
			auto key = line.substr(0, split);
			auto val = line.substr(split+1);
			trim(key);
			trim(val);
			(*kv)[std::string(key)] = std::string(val);
		}
	}

	return sections;
}


std::string config::location()
{
	if constexpr (Current_Platform == Platform::Linux) {
		if (auto config_dir = getenv("XDG_CONFIG_HOME")) {
			return config_dir + std::string("/musique.conf");
		} else if (auto home_dir = getenv("HOME")) {
			return std::string(home_dir) + "/.config/musique.conf";
		} else {
			unimplemented("Neither XDG_CONFIG_HOME nor HOME enviroment variable are defined");
		}
	} else {
		unimplemented();
	}
}
