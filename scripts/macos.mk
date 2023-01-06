CC=clang
CXX=clang++
CPPFLAGS:=$(CPPFLAGS) -D __MACOSX_CORE__ -D LINK_PLATFORM_MACOSX
LDLIBS:=-framework CoreMIDI -framework CoreAudio -framework CoreFoundation $(LDLIBS)
Release_Obj=$(addprefix bin/,$(Obj))
Bestline=bin/$(os)/bestline.o
Target=musique
