MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"
CXXFLAGS=-std=c++20 -Wall -Wextra -O2 -Werror=switch
CPPFLAGS=-Ilib/expected/ -Ilib/ut/

Obj=bin/lexer.o \
		bin/errors.o \
		bin/main.o

all: bin/musique

bin/%.o: src/%.cc src/*.hh
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Obj) src/*.hh
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $(Obj)

clean:
	rm -rf bin

.PHONY: clean

$(shell mkdir -p bin)
