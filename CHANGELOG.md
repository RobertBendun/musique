# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- new builtins: map, while, set_len, set_oct, duration, pick
- `<note><octave>` notation like `c4` that mimics [scientific notation](https://en.wikipedia.org/wiki/Scientific_pitch_notation)
- operator pretty printing while printing values
- macros: builtin functions that takes AST and produces value
- early text-based snapshot system available via `:snap` command inside interactive session
- polish basic tutorial

### Changed

- Block can be called with more parameters then it requires
- reorganized fold argument order
- Moved from `(<note> <octave> <length>)` invocation to `(<note> <length>)`
- Moved from '[]' to '()' notation for blocks
- Moved from `a.n` index operator to `a[n]`
- Moved from ';' to ',' notation for expression separator
- Moved 'if', 'while' from beeing functions to macros - side effect of new notation
- Build system uses now Docker
- Array repetition using `number * array` like `3 * (c, e) == (c, e, c, e, c, e)`

### Fixed

- Index operation using booleans behaves like a mask and not fancy way of spelling 0 and 1
- Blocks are check against beeing a collection at runtime to prevent treating anonymous functions as collections and cousing assertions
- On Windows default terminal emulator ansi escape codes are conditionally supported. Review musique/pretty.cc for details

### Removed

- Removed `<note><absolute notes indexes>` chord notation like `c47` meaning c-major
- Removed obsolete documentation
- `h` as note literal

## [0.2.1] - 2022-10-21

### Fixed

- Windows build doesn't require pthread dll

## [0.2.0] - 2022-10-21

### Added

* Added `scan` builtin, which computes prefix sum of passed values when provided with addition operator
* Added [rtmidi](https://github.com/thestk/rtmidi/) dependency which should provide multiplatform MIDI support
* Support for Windows (with only basic REPL) (`make os=windows`)
* Support for MacOS (`make os=macos`)
* Release package now with compiled Windows binary
* `:load` REPL command to load Musique files inside Musique session. Allows for delayed file execution after a connection
* `:quit` REPL command that mirrors `:exit` command
* Virtual MIDI output port creation as default action (--output connects to existing one)
* Added build instructions
* `-f` commandline argument that will turn file into deffered function

### Changed

* Integrated libmidi library into Musique codebase
* Moved from custom ALSA interaction to using rtmidi for MIDI I/O operations
* VSCode extension has been re-created

### Removed

* Support for incoming MIDI messages handling due to poor implementation that didn't statisfy user needs

### Fixed

* Prevented accidental recursive construction of Arrays and Values by making convinience constructor `Value::Value(std::vector<Value>&&)` explicit

## [0.1.0] - 2022-09-25

### Added

* Musique programming language initial implementation that supports:
 * Chord system
 * Playing MIDI notes using par, sim and play
 * Notes and chords as first-class citizens of Musique
 * Bunch of builtins like math and array operations
 * All numerical values as fractions (like in JavaScript but better)
 * Primitive interactive mode
 * Only ALSA MIDI Sequencer output
* Simple regression testing framework
* Basic documentation of builtin functions and operators
