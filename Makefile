include config.mk

Sources := $(shell find musique/ -name '*.cc')
Obj := $(subst musique/,,$(Sources:%.cc=%.o))

all: bin/musique

include scripts/debug.mk
include scripts/release.mk
include scripts/test.mk
include scripts/windows.mk

# http://www.music.mcgill.ca/~gary/rtmidi/#compiling
bin/rtmidi.o: lib/rtmidi/RtMidi.cpp lib/rtmidi/RtMidi.h
	@echo "CXX $@"
	@$(CXX) $< -c -O2 -o $@ $(CPPFLAGS) -std=c++20

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
