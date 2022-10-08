# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

* Added `scan` builtin, which computes prefix sum of passed values when provided with addition operator
* Added [rtmidi](https://github.com/thestk/rtmidi/) dependency which should provide multiplatform MIDI support

### Changed

* Integrated libmidi library into Musique codebase
* Moved from custom ALSA interaction to using rtmidi for MIDI I/O operations

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
