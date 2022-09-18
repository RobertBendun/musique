include config.mk

Obj=                    \
		builtin_functions.o \
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


all: bin/musique

include scripts/debug.mk
include scripts/release.mk
include scripts/test.mk

bin/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

doc: Doxyfile src/*.cc include/*.hh
	doxygen

doc-open: doc
	xdg-open ./doc/build/html/index.html

clean:
	rm -rf bin coverage

release: bin/musique
	scripts/release



.PHONY: clean doc doc-open all test unit-tests release

$(shell mkdir -p bin/debug/tests)
