CC=gcc
CXX=g++
CPPFLAGS:=$(CPPFLAGS) -D __LINUX_ALSA__ -D LINK_PLATFORM_LINUX
LDLIBS:=-lasound -lrt $(LDLIBS) -static-libgcc -static-libstdc++
Bestline=bin/$(os)/bestline.o
Target=musique
Serial_List_Ports=list_ports_linux
Serial_Impl=unix

