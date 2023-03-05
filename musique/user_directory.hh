#ifndef MUSIQUE_USER_DIRECTORY_HH
#define MUSIQUE_USER_DIRECTORY_HH

#include <filesystem>

namespace user_directory
{
	/// Returns system-specific user directory for data; same as XDG_DATA_HOME
	std::filesystem::path data_home();

	/// Returns system-specific user directory for config; same as XDG_CONFIG_HOME
	std::filesystem::path config_home();
}

#endif // MUSIQUE_USER_DIRECTORY_HH
