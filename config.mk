MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result -Wno-maybe-uninitialized
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -I. -Ilib/bestline/ -Ilib/rtmidi/
LDFLAGS=-flto
LDLIBS= -lpthread -static-libgcc -static-libstdc++

RELEASE_FLAGS=-O2
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug

ifeq ($(os),windows)
CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CPPFLAGS:=$(CPPFLAGS) -D__WINDOWS_MM__
LDLIBS:=-lwinmm $(LDLIBS)
else
CC=gcc
CXX=g++
CPPFLAGS:=$(CPPFLAGS) -D __LINUX_ALSA__
LDLIBS:=-lasound $(LDLIBS)
endif

