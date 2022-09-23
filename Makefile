include config.mk

Sources := $(shell find musique/ -name '*.cc')
Obj := $(subst musique/,,$(Sources:%.cc=%.o))

all: bin/musique

include scripts/debug.mk
include scripts/release.mk
include scripts/test.mk

bin/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

doc: Doxyfile musique/*.cc musique/*.hh
	doxygen

doc-open: doc
	xdg-open ./doc/build/html/index.html

clean:
	rm -rf bin coverage

release: bin/musique
	scripts/release

install: bin/musique
	scripts/install

.PHONY: clean doc doc-open all test unit-tests release install

$(shell mkdir -p $(subst musique/,bin/,$(shell find musique/* -type d)))
$(shell mkdir -p $(subst musique/,bin/debug/,$(shell find musique/* -type d)))
