CC=gcc
CXX=g++
CPPFLAGS:=$(CPPFLAGS) -D __LINUX_ALSA__ -D LINK_PLATFORM_LINUX
LDLIBS:=-lasound $(LDLIBS) -static-libgcc -static-libstdc++
Target=musique
