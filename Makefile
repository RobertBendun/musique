MAKEFLAGS="-j $(grep -c ^processor /proc/cpuinfo)"
CXXFLAGS:=$(CXXFLAGS) -std=c++20 -Wall -Wextra -Werror=switch -Werror=unused-result
CPPFLAGS:=$(CPPFLAGS) -Ilib/expected/ -Ilib/ut/ -Isrc/

Obj=bin/errors.o        \
		bin/lexer.o         \
		bin/location.o      \
		bin/parser.o        \
		bin/unicode.o       \
		bin/unicode_tables.o

all: bin/musique bin/unit-tests

bin/%.o: src/%.cc src/*.hh
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Obj) bin/main.o src/*.hh
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $(Obj) bin/main.o

.PHONY: unit-tests
unit-tests: bin/unit-tests
	./$<

unit-test-coverage:
	@which gcov >/dev/null || ( echo "[ERROR] gcov is required for test coverage report"; false )
	@which gcovr >/dev/null || ( echo "[ERROR] gcovr is required for test coverage report"; false )
	CXXFLAGS=--coverage $(MAKE) bin/unit-tests -B
	bin/unit-tests
	rm -rf coverage
	mkdir coverage
	gcovr -e '.*\.hpp' --html --html-details -o coverage/index.html
	rm -rf bin

bin/unit-tests: src/tests/*.cc $(Obj)
	g++ $(CXXFLAGS) $(CPPFLAGS) -o $@ $^

clean:
	rm -rf bin coverage

.PHONY: clean

$(shell mkdir -p bin)
