MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result -Wno-maybe-uninitialized
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -I. -Ilib/bestline/ -Ilib/rtmidi/

RELEASE_FLAGS=-O2
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug

CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++

LDFLAGS=-flto
LDLIBS=-lwinmm -lpthread -static-libgcc -static-libstdc++
