#ifndef MUSIQUE_VALUE_CHORD_HH
#define MUSIQUE_VALUE_CHORD_HH

#include <vector>

#include <musique/value/note.hh>
#include <musique/value/function.hh>

struct Interpreter;
struct Value;

/// Represantation of simultaneously played notes, aka musical chord
struct Chord : Function
{
	std::vector<Note> notes; ///< Notes composing a chord

	Chord() = default;
	explicit Chord(Note note);
	explicit Chord(std::vector<Note> &&notes);

	/// Parse chord literal from provided source
	static Chord from(std::string_view source);

	bool operator==(Chord const&) const = default;

	/// Fill length and octave or sequence multiple chords
	Result<Value> operator()(Interpreter &i, std::vector<Value> args) const override;
};

std::ostream& operator<<(std::ostream& os, Chord const& chord);

#endif // MUSIQUE_VALUE_CHORD_HH
