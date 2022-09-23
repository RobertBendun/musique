#ifndef MUSIQUE_NOTE_HH
#define MUSIQUE_NOTE_HH

#include <optional>

#include "number.hh"

/// Representation of musical note or musical pause
struct Note
{
	/// Base of a note, like `c` (=0), `c#` (=1) `d` (=2)
	/// Or nullopt where there is no note - case when we have pause
	std::optional<i32> base = std::nullopt;

	/// Octave in MIDI acceptable range (from -1 to 9 inclusive)
	std::optional<i8> octave = std::nullopt;

	/// Length of playing note
	std::optional<Number> length = std::nullopt;

	/// Create Note from string
	static std::optional<Note> from(std::string_view note);

	/// Extract midi note number
	std::optional<u8> into_midi_note() const;

	/// Extract midi note number, but when octave is not present use provided default
	u8 into_midi_note(i8 default_octave) const;

	bool operator==(Note const&) const;

	std::partial_ordering operator<=>(Note const&) const;

	/// Simplify note by adding base to octave if octave is present
	void simplify_inplace();
};

std::ostream& operator<<(std::ostream& os, Note note);

#endif
