include config.mk

Sources := $(shell find musique/ -name '*.cc')
Obj := $(subst musique/,,$(Sources:%.cc=%.o))

include scripts/$(os).mk
include scripts/build.mk

all: $(PREFIX)/$(Target)

full: doc/musique-vs-languages-cheatsheet.html doc/wprowadzenie.html doc/functions.html
	make all os=$(os)
	make all os=$(os) mode=debug
	make all os=$(os) mode=unit-test

bin/$(Target): bin/$(os)/$(Target)
	ln -f $< $@

doc: Doxyfile musique/*.cc musique/*.hh
	doxygen

doc-open: doc
	xdg-open ./doc/build/html/index.html

clean:
	rm -rf bin coverage

install: bin/musique
	scripts/install

doc/musique-vs-languages-cheatsheet.html: doc/musique-vs-languages-cheatsheet.template
	scripts/language-cmp-cheatsheet.py $<

doc/wprowadzenie.html: doc/wprowadzenie.md
	pandoc -o $@ $< -s --toc

doc/functions.html: musique/interpreter/builtin_functions.cc scripts/document-builtin.py
	scripts/document-builtin.py -o $@ -f html $<

musique.zip:
	docker build -t musique-builder --build-arg "VERSION=$(VERSION)" .
	docker create --name musique musique-builder
	docker cp musique:/musique.zip musique.zip
	docker rm -f musique

test:
	make mode=debug
	python3 scripts/test.py

.PHONY: clean doc doc-open all test unit-tests release install musique.zip full release

$(shell mkdir -p bin/$(os)/replxx/)
$(shell mkdir -p $(subst musique/,$(PREFIX)/,$(shell find musique/* -type d)))
