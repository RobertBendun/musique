# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Types of changes

- __Added__ for new features.
- __Changed__ for changes in existing functionality.
- __Deprecated__ for soon-to-be removed features.
- __Removed__ for now removed features.
- __Fixed__ for any bug fixes.
- __Security__ in case of vulnerabilities.


## [Unreleased]

## [0.6.0] - 2023-06-09

### Added

- Builtin `seed` to provide seed for all functions using random number generation
- New `make` target: `full` - make everything that can be made for local developement
- `time` REPL command for measurement of execution time
- Unit testing strikes again! `make mode=unit-test`
- Continuous Integration with Github Actions
- [Musical Symbols Unicode block](https://en.wikipedia.org/wiki/Musical_Symbols_(Unicode_block)) can be used as an identifiers
    - by default they are lengths of notes and rests appropiate for given symbol

### Changed

- Simplified `make` definition, `make mode=debug` for debug builds
- More regression tests

### Fixed

- function documentation generator proper cmdline parameter handling
- deterministic random number generation is now cross platform (setting the same seed on different platforms gives the same results)
- `nprimes 1` returns correctly `(2)`

## [0.5.0] - 2023-03-05

### Added

- Builtin documentation for builtin functions display from repl and command line (`musique doc <builtin>`)
- Man documentation for commandline interface builtin (`musique man`)
- Suggestions which command line parameters user may wanted to use

### Changed

- Moved from `bestline` to `replxx` Readline implementation due to lack of Windows support from bestline
- New parameter passing convention for command line invocation. `musique help` to learn how it changed
- `CTRL-C` handler that turns notes that are playing off

## [0.4.0] - 2023-02-15

### Added

- Builtin function documentation generation from C++ Musique implementation source code
- New builtins: digits
- Negative numbers!
- Version command via `:version` in REPL or `--version`, `-v` in command line.
- Introduced start synchronization with builtins: `peers` and `start`
- Connection with MIDI ports via parameters dropped in favour of function using context system: `port`
- Listing ports via REPL command: `:ports` instead of commandline parameter
- Building release with `make musique.zip` using Docker

### Changed

- Readme from Polish to English

### Removed

- Release script builder

### Fixed

- `ceil`, `round`, `floor` didn't behave well with negative numbers
- `duration` wasn't filling note length from context and summed all notes inside chord, when it should take max
- `try` evaluated arguments too quickly
- Nested arithmetic expression with undefined operator reports proper error and don't crash

## [0.3.1] - 2022-11-28

### Fixed

- release build script was producing executable with wrong path
- `examples/for-elise.mq` had bug in it
- in error reporting printing garbage instead of function name was fixed
- sending proper program change message

## [0.3.0] - 2022-11-21

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
