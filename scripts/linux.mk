CC=gcc
CXX=g++
CPPFLAGS:=$(CPPFLAGS) -D __LINUX_ALSA__
LDLIBS:=-lasound -lrt $(LDLIBS) -static-libgcc -static-libstdc++
Bestline=bin/$(os)/bestline.o
Target=musique

