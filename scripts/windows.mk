CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CPPFLAGS:=$(CPPFLAGS) -D__WINDOWS_MM__
LDLIBS:=-lsetupapi -lhid -lwinmm $(LDLIBS) -static-libgcc -static-libstdc++ -static
Target=musique.exe
Serial_List_Ports=list_ports_win
Serial_Impl=win
