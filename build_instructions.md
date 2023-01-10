# Musique build instructions

## Linux

Use your local C++ compiler (supporting C++20) and ALSA dev libraries.

Or included dockerfile:
```
bash> docker build -t musique-builder .
bash> docker run -it -v "$PWD:/src" musique-builder:latest
docker> make CC=gcc-11 CXX=g++-11
```

## MacOS

Required system dependency is fairly new C++ compiler. The simplest way is to type `clang` in terminal, which should launch installation of C++ compiler. Next provided compiler version:
```
$ clang++ --version
```

Minimal supported version is __14__. On older editions of MacOS one can aquire new editions by [Homebrew](https://brew.sh/). After homebrew installation install clang via `brew install llvm`.

Next you should be able to build project by running `make` command inside main directory (in released zip file in `source_code` directory).

## Windows

Windows support is provided via cross compilation. Mingw GCC C++ compiler supporting C++20 is required to produce Windows binaries.

```
$ make os=windows
```

will create `bin/musique.exe` that can be used on x86_64 Windows operating systems.

