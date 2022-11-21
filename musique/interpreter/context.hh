#ifndef MUSIQUE_CONTEXT_HH
#define MUSIQUE_CONTEXT_HH

#include <musique/common.hh>
#include <musique/value/note.hh>
#include <musique/value/number.hh>
#include <chrono>
#include <memory>

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

	std::shared_ptr<Context> parent;
};

#endif
