#ifndef MUSIQUE_CONTEXT_HH
#define MUSIQUE_CONTEXT_HH

#include "common.hh"
#include "note.hh"
#include "number.hh"
#include <chrono>

/// Context holds default values for music related actions
struct Context
{
	/// Default note octave
	i8 octave = 4;

	/// Default note length
	Number length = Number(1, 4);

	/// Default BPM
	unsigned bpm = 120;

	/// Fills empty places in Note like octave and length with default values from context
	Note fill(Note) const;

	/// Converts length to seconds with current bpm
	std::chrono::duration<float> length_to_duration(std::optional<Number> length) const;
};

#endif
