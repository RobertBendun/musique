#include <fstream>
#include <iterator>
#include <musique/config.hh>
#include <musique/errors.hh>
#include <musique/platform.hh>
#include <iostream>

#ifdef MUSIQUE_WINDOWS
#include <shlobj.h>
#include <knownfolders.h>
#include <cstring>
#endif

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

void config::to_file(std::string const& path, Sections const& sections)
{
	std::ofstream out(path, std::ios_base::trunc | std::ios_base::out);
	if (!out.is_open()) {
		std::cout << "Failed to save configuration at " << path << std::endl;
		return;
	}

	for (auto const& [section, kv] : sections) {
		out << '[' << section << "]\n";
		for (auto const& [key, val] : kv) {
			out << key << " = " << val << '\n';
		}
	}
}


std::string config::location()
{
#ifdef MUSIQUE_LINUX
		if (auto config_dir = getenv("XDG_CONFIG_HOME")) {
			return config_dir + std::string("/musique.conf");
		} else if (auto home_dir = getenv("HOME")) {
			return std::string(home_dir) + "/.config/musique.conf";
		} else {
			unimplemented("Neither XDG_CONFIG_HOME nor HOME enviroment variable are defined");
		}
#endif

#ifdef MUSIQUE_WINDOWS
		wchar_t *resultPath = nullptr;
		SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &resultPath);
		auto const len = std::strlen((char*)resultPath);
		if (!resultPath && len < 3) {
			auto drive = getenv("HOMEDRIVE");
			auto path = getenv("HOMEPATH");
			ensure(drive && path, "Windows failed to provide HOMEDRIVE & HOMEPATH variables");
			return std::string(drive) + std::string(path) + "\\Documents\\musique.conf";
		}

		return std::string((char*)resultPath, len) + "\\musique.conf";
#endif

#ifdef MUSIQUE_DARWIN
		if (auto home_dir = getenv("HOME")) {
			return std::string(home_dir) + "/Library/Application Support/musique.conf";
		} else {
			unimplemented("HOME enviroment variable is not defined");
		}
#endif

		unimplemented();
}
