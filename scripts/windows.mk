CC=x86_64-w64-mingw32-cc
CXX=x86_64-w64-mingw32-c++
CPPFLAGS:=$(CPPFLAGS) -D__WINDOWS_MM__
LDLIBS:=-lwinmm $(LDLIBS) -static-libgcc -static-libstdc++ -static
Target=musique.exe
GOOS=windows
GOARCH=amd64
