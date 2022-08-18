MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"
CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result -Wno-maybe-uninitialized
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -Ilib/ut/ -Ilib/midi/include -Isrc/ -Ilib/bestline/
RELEASE_FLAGS=-O3
DEBUG_FLAGS=-O0 -ggdb -fsanitize=undefined -DDebug
CXX=g++

LDFLAGS=-L./lib/midi/
LDLIBS=-lmidi-alsa -lasound -lpthread

Obj=                    \
		builtin_operators.o \
		context.o           \
		environment.o       \
		errors.o            \
		interpreter.o       \
		lexer.o             \
		lines.o             \
		location.o          \
		number.o            \
		parser.o            \
		pretty.o            \
		unicode.o           \
		unicode_tables.o    \
		value.o

Tests=          \
	context.o     \
	environment.o \
	interpreter.o \
	lex.o         \
	main.o        \
	number.o      \
	parser.o      \
	unicode.o     \
	value.o

Release_Obj=$(addprefix bin/,$(Obj))
Debug_Obj=$(addprefix bin/debug/,$(Obj))
Test_Obj=$(addprefix bin/debug/tests/,$(Tests))

all: bin/musique

test: unit-tests
	etc/tools/test.py test examples

debug: bin/debug/musique

bin/%.o: src/%.cc src/*.hh
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Release_Obj) bin/main.o bin/bestline.o src/*.hh lib/midi/libmidi-alsa.a
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/bestline.o bin/main.o $(LDFLAGS) $(LDLIBS)

bin/debug/musique: $(Debug_Obj) bin/debug/main.o bin/bestline.o src/*.hh
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/bestline.o bin/debug/main.o  $(LDFLAGS) $(LDLIBS)

bin/debug/%.o: src/%.cc src/*.hh
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

unit-tests: bin/unit-tests
	./$<

unit-test-coverage:
	@which gcov >/dev/null || ( echo "[ERROR] gcov is required for test coverage report"; false )
	@which gcovr >/dev/null || ( echo "[ERROR] gcovr is required for test coverage report"; false )
	CXXFLAGS=--coverage $(MAKE) bin/unit-tests -B
	bin/unit-tests
	rm -rf coverage
	mkdir coverage
	gcovr -e '.*\.hpp' -e 'src/tests/.*' -e 'src/pretty.cc' --html --html-details -o coverage/index.html
	rm -rf bin/debug
	xdg-open coverage/index.html

doc: Doxyfile src/*.cc src/*.hh
	doxygen

doc-open: doc
	xdg-open ./doc/build/html/index.html

bin/unit-tests: $(Test_Obj) $(Debug_Obj)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEBUG_FLAGS) -o $@ $^

clean:
	rm -rf bin coverage

.PHONY: clean doc doc-open all test unit-tests

$(shell mkdir -p bin/debug/tests)
