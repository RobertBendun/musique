all: libmidi-alsa.a

midi.o: midi.cc midi.hh
	g++ -c -std=c++20 -Wall -Wextra $< -o $@ -I.

midi-alsa.o: alsa.cc midi.hh
	g++ -c -std=c++20 -Wall -Wextra $< -o $@ -I.

libmidi-alsa.a: midi.o midi-alsa.o
	ar rcs $@ $^

example-usage-alsa: example-usage-alsa.cc libmidi-alsa.a
	g++ -std=c++20 -Wall -Wextra $< -o $@ -L. -lmidi-alsa -lasound -Iinclude -g
