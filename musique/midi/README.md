# `midi` sub library

Utility library for interfacing MIDI protocol in different environments.

## Available interfaces

### ALSA

To use with ALSA before including library define `MIDI_ENABLE_ALSA_SUPPORT` macro.

To compile static library use `make libmidi-alsa`.

When in doubt, see [`example-usage-alsa.cc`](./example-usage-alsa.cc).
