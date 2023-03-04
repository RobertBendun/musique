#include <musique/user_directory.hh>
#include <musique/errors.hh>
#include <musique/platform.hh>

static std::filesystem::path home()
{
	if constexpr (platform::os == platform::Operating_System::Windows) {
		if (auto home = std::getenv("USERPROFILE")) return home;

		if (auto drive = std::getenv("HOMEDRIVE")) {
			if (auto path = std::getenv("HOMEPATH")) {
				return std::string(drive) + path;
			}
		}
	} else {
		if (auto home = std::getenv("HOME")) {
			return home;
		}
	}

	unreachable();
}

std::filesystem::path user_directory::data_home()
{
	std::filesystem::path path;

	static_assert(one_of(platform::os,
		platform::Operating_System::Unix,
		platform::Operating_System::Windows,
		platform::Operating_System::MacOS
	));

	if constexpr (platform::os == platform::Operating_System::Unix) {
		if (auto data = std::getenv("XDG_DATA_HOME")) {
			path = data;
		} else {
			path = home() / ".local" / "share";
		}
	}

	if constexpr (platform::os == platform::Operating_System::Windows) {
		if (auto data = std::getenv("LOCALAPPDATA")) {
			path = data;
		} else {
			path = home() / "AppData" / "Local";
		}
	}

	if constexpr (platform::os == platform::Operating_System::MacOS) {
		path = home() / "Library";
	}

	path /= "musique";
	std::filesystem::create_directories(path);
	return path;
}

std::filesystem::path user_directory::config_home()
{
	std::filesystem::path path;

	static_assert(one_of(platform::os,
		platform::Operating_System::Unix,
		platform::Operating_System::Windows,
		platform::Operating_System::MacOS
	));

	if constexpr (platform::os == platform::Operating_System::Unix) {
		if (auto data = std::getenv("XDG_CONFIG_HOME")) {
			path = data;
		} else {
			path = home() / ".config";
		}
	}

	if constexpr (platform::os == platform::Operating_System::Windows) {
		if (auto data = std::getenv("LOCALAPPDATA")) {
			path = data;
		} else {
			path = home() / "AppData" / "Local";
		}
	}

	if constexpr (platform::os == platform::Operating_System::MacOS) {
		path = home() / "Library" / "Preferences";
	}

	path /= "musique";
	std::filesystem::create_directories(path);
	return path;
}
