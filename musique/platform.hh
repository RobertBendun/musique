#ifndef MUSIQUE_PLATFORM_HH
#define MUSIQUE_PLATFORM_HH

enum class Platform
{
	Darwin,
	Linux,
	Windows,
};

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
	static constexpr Platform Current_Platform = Platform::Windows;
#define MUSIQUE_WINDOWS
#elif __APPLE__
	static constexpr Platform Current_Platform = Platform::Darwin;
#define MUSIQUE_DARWIN
#else
  static constexpr Platform Current_Platform = Platform::Linux;
#define MUSIQUE_LINUX
#endif

#endif // MUSIQUE_PLATFORM_HH
