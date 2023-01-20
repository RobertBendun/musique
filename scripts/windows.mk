CC=i686-w64-mingw32-gcc
CXX=i686-w64-mingw32-g++
CPPFLAGS:=$(CPPFLAGS) -D__WINDOWS_MM__ -D LINK_PLATFORM_WINDOWS
LDLIBS:=-lsetupapi -lhid -lwinmm -liphlpapi -lws2_32 $(LDLIBS) -static-libgcc -static-libstdc++ -static
Target=musique.exe
Serial_List_Ports=list_ports_win
Serial_Impl=win
