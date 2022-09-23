#include <vector>

#include "note.hh"

struct Interpreter;
struct Value;

/// Represantation of simultaneously played notes, aka musical chord
struct Chord
{
	std::vector<Note> notes; ///< Notes composing a chord

	/// Parse chord literal from provided source
	static Chord from(std::string_view source);

	bool operator==(Chord const&) const = default;

	/// Fill length and octave or sequence multiple chords
	Result<Value> operator()(Interpreter &i, std::vector<Value> args);
};

std::ostream& operator<<(std::ostream& os, Chord const& chord);
