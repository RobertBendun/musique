MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result -Wno-maybe-uninitialized
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -Ilib/ut/ -Ilib/midi/include -Iinclude/ -Ilib/bestline/

RELEASE_FLAGS=-O3
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug

CXX=g++

LDFLAGS=-L./lib/midi/
LDLIBS=-lmidi-alsa -lasound -lpthread
