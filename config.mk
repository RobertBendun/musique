MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -I. -Ilib/bestline/ -Ilib/rtmidi/ -Ilib/serial/include/
LDFLAGS=-flto
LDLIBS= -lpthread

RELEASE_FLAGS=-O2
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug

ifeq ($(shell uname),Darwin)
os=macos
else
os=linux
endif

