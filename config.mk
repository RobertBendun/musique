MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"

MAJOR := 0
MINOR := 5
PATCH := 0
COMMIT := gc$(shell git rev-parse --short HEAD 2>/dev/null)

ifeq ($(COMMIT),gc)
	COMMIT = gcunknown
endif

VERSION := $(MAJOR).$(MINOR).$(PATCH)-dev+$(COMMIT)

CXXFLAGS := $(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result
CPPFLAGS := $(CPPFLAGS) -DMusique_Version='"$(VERSION)"' \
	-Ilib/expected/ -I. -Ilib/rtmidi/ -Ilib/link/include -Ilib/asio/include/ -Ilib/edit_distance.cc/ -Ilib/replxx/include  -DREPLXX_STATIC
LDFLAGS =-flto=auto
LDLIBS = -lpthread

RELEASE_FLAGS=-O2
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug

ifeq ($(shell uname),Darwin)
os=macos
else
os=linux
endif

ifeq ($(mode),debug)

PREFIX = bin/$(os)/debug
CXXFLAGS += $(DEBUG_FLAGS)
MAIN = $(PREFIX)/main.o

else ifeq ($(mode),unit-test)

PREFIX = bin/$(os)/unit-test
CXXFLAGS += $(DEBUG_FLAGS) -DMUSIQUE_UNIT_TESTING
CPPFLAGS += -Ilib/Catch2/
MAIN = lib/Catch2/catch_amalgamated.cpp

else

PREFIX = bin/$(os)
CXXFLAGS += $(RELEASE_FLAGS)
MAIN = $(PREFIX)/main.o

endif


