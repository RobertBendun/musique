MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"
CXXFLAGS=-std=c++20 -Wall -Wextra -O2 -Werror=switch
CPPFLAGS=-Ilib/expected/ -Ilib/ut/ -Isrc/

Obj=bin/errors.o \
		bin/lexer.o \
		bin/unicode.o

all: bin/musique bin/unit-tests


bin/%.o: src/%.cc src/*.hh
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Obj) bin/main.o src/*.hh
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $(Obj) bin/main.o

.PHONY: unit-tests
unit-tests: bin/unit-tests
	./$<

bin/unit-tests: src/tests/*.cc $(Obj)
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $^

clean:
	rm -rf bin

.PHONY: clean

$(shell mkdir -p bin)
