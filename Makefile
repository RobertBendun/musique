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

doc/musique-vs-languages-cheatsheet.html: doc/musique-vs-languages-cheatsheet.template
	scripts/language-cmp-cheatsheet.py $<

doc/wprowadzenie.html: doc/wprowadzenie.md
	pandoc -o $@ $< -s --toc

doc/functions.html: musique/interpreter/builtin_functions.cc scripts/document-builtin.py
	scripts/document-builtin.py -o $@ $<

musique.zip:
	docker build -t musique-builder --build-arg "VERSION=$(VERSION)" .
	docker create --name musique musique-builder
	docker cp musique:/musique.zip musique.zip
	docker rm -f musique

.PHONY: clean doc doc-open all test unit-tests release install musique.zip

$(shell mkdir -p $(subst musique/,bin/$(os)/,$(shell find musique/* -type d)))
$(shell mkdir -p bin/$(os)/replxx/)
$(shell mkdir -p $(subst musique/,bin/$(os)/debug/,$(shell find musique/* -type d)))
