#ifndef MUSIQUE_PLATFORM_HH
#define MUSIQUE_PLATFORM_HH

namespace platform
{
	enum class Operating_System
	{
		MacOS,
		Unix,
		Windows,
	};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	static constexpr Operating_System os = Operating_System::Windows;
#elif defined(__APPLE__)
	static constexpr Operating_System os = Operating_System::MacOS;
#elif defined(__linux__) || defined(__unix__)
	static constexpr Operating_System os = Operating_System::Unix;
#else
#error "Unknown platform"
#endif
}

#endif // MUSIQUE_PLATFORM_HH
