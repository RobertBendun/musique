MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result -Wno-maybe-uninitialized
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -I. -Ilib/bestline/ -Ilib/rtmidi/

RELEASE_FLAGS=-O2
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug

CXX=g++

LDFLAGS=-flto
LDLIBS=-lasound -lpthread -static-libgcc -static-libstdc++
