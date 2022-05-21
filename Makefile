MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"
CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=return-type -Werror=unused-result
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -Ilib/ut/ -Isrc/
RELEASE_FLAGS=-O3
DEBUG_FLAGS=-O0 -ggdb

Obj=                 \
		environment.o    \
		errors.o         \
		interpreter.o    \
		lexer.o          \
		location.o       \
		number.o         \
		parser.o         \
		unicode.o        \
		unicode_tables.o \
		value.o

Release_Obj=$(addprefix bin/,$(Obj))
Debug_Obj=$(addprefix bin/debug/,$(Obj))

all: bin/musique

debug: bin/debug/musique

bin/%.o: src/%.cc src/*.hh
	g++ $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Release_Obj) bin/main.o src/*.hh
	g++ $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/main.o

bin/debug/musique: $(Debug_Obj) bin/debug/main.o src/*.hh
	g++ $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/debug/main.o

bin/debug/%.o: src/%.cc src/*.hh
	g++ $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c

.PHONY: unit-tests
unit-tests: bin/unit-tests
	./$<

.PHONY: unit-test-coverage
unit-test-coverage:
	@which gcov >/dev/null || ( echo "[ERROR] gcov is required for test coverage report"; false )
	@which gcovr >/dev/null || ( echo "[ERROR] gcovr is required for test coverage report"; false )
	CXXFLAGS=--coverage $(MAKE) bin/unit-tests -B
	bin/unit-tests
	rm -rf coverage
	mkdir coverage
	gcovr -e '.*\.hpp' --html --html-details -o coverage/index.html
	rm -rf bin
	xdg-open coverage/index.html

.PHONY: doc
doc: Doxyfile src/*.cc src/*.hh
	doxygen
	cd doc; $(MAKE) html

.PHONY: doc-open
doc-open: doc
	xdg-open ./doc/build/html/index.html

bin/unit-tests: src/tests/*.cc $(Debug_Obj)
	g++ $(CXXFLAGS) $(CPPFLAGS) $(DEBUG_FLAGS) -o $@ $^

clean:
	rm -rf bin coverage

.PHONY: clean

$(shell mkdir -p bin/debug)
