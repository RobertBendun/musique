include config.mk

Sources := $(shell find musique/ -name '*.cc')
Obj := $(subst musique/,,$(Sources:%.cc=%.o))

ifeq ($(os),windows)
all: bin/musique.exe
debug: bin/windows/debug/musique.exe
else
all: bin/musique
debug: bin/$(os)/debug/musique
endif

include scripts/$(os).mk
include scripts/build.mk
include scripts/test.mk

bin/$(Target): bin/$(os)/$(Target)
	ln -f $< $@

# http://www.music.mcgill.ca/~gary/rtmidi/#compiling
bin/$(os)/rtmidi.o: lib/rtmidi/RtMidi.cpp lib/rtmidi/RtMidi.h
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

$(shell mkdir -p $(subst musique/,bin/$(os)/,$(shell find musique/* -type d)))
$(shell mkdir -p $(subst musique/,bin/$(os)/debug/,$(shell find musique/* -type d)))
