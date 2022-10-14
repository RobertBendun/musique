CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CPPFLAGS:=$(CPPFLAGS) -D__WINDOWS_MM__
LDLIBS:=-lwinmm $(LDLIBS) -static-libgcc -static-libstdc++

