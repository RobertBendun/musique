workspace "Musique"
	configurations { "Debug", "Release" }
	flags { "MultiProcessorCompile" }
	makesettings { 'MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"' }

project "Musique"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	includedirs { ".", "lib/bestline/", "lib/rtmidi/", "lib/expected/" }
	files { "musique/**.hh", "musique/**.cc" }
	links { "bestline", "rtmidi" }

	filter "configurations:Debug"
		defines { "Debug" }
		symbols "On"
		targetdir "bin/debug"
		objdir "bin/debug"
	filter {}

	filter { "system:linux", "configurations:Debug" }
		buildoptions { "-Og", "-ggdb", "-fsanitize=undefined" }
	filter {}

	filter "configurations:Release"
		targetdir "bin/"
		objdir "bin/"
		optimize "On"
	filter {}

	filter "system:windows"
		targetname "musique.exe"
	filter {}

	filter "system:linux"
		links { "asound", "pthread" }
		targetname "musique"
	filter {}

project "Bestline"
	kind "StaticLib"
	language "C"

	includedirs { "lib/bestline" }
	files { "lib/bestline/bestline.c" }
	targetdir "bin/"
	optimize "On"

	filter "configurations:Debug"
		targetdir "bin/debug"
		objdir "bin/debug"
	filter {}

	filter "configurations:Release"
		targetdir "bin/"
		objdir "bin/"
	filter {}

project "RtMidi"
	kind "StaticLib"
	language "C++"

	includedirs { "lib/rtmidi" }
	files { "lib/rtmidi/RtMidi.cpp" }
	targetdir "bin/"
	optimize "On"

	filter "system:linux"
		defines { "__LINUX_ALSA__" }
	filter {}

	filter "configurations:Debug"
		targetdir "bin/debug"
		objdir "bin/debug"
	filter {}

	filter "configurations:Release"
		targetdir "bin/"
		objdir "bin/"
	filter {}
