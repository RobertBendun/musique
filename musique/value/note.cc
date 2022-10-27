#include <musique/value/note.hh>
#include <charconv>

/// Finds numeric value of note. This form is later used as in
/// note to midi resolution in formula octave * 12 + note_index
constexpr u8 note_index(u8 note)
{
	switch (note) {
	case 'c':  return  0;
	case 'd':  return  2;
	case 'e':  return  4;
	case 'f':  return  5;
	case 'g':  return  7;
	case 'a':  return  9;
	case 'h':  return 11;
	case 'b':  return 11;
	}
	// Parser should limit range of characters that is called with this function
	unreachable();
}

constexpr std::string_view note_index_to_string(int note_index)
{
	note_index %= 12;
	if (note_index < 0) {
		note_index = 12 + note_index;
	}

	switch (note_index) {
	case  0:	return "c";
	case  1:	return "c#";
	case  2:	return "d";
	case  3:	return "d#";
	case  4:	return "e";
	case  5:	return "f";
	case  6:	return "f#";
	case  7:	return "g";
	case  8:	return "g#";
	case  9:	return "a";
	case 10:	return "a#";
	case 11:	return "b";
	}
	unreachable();
}

std::optional<Note> Note::from(std::string_view literal)
{
	std::optional<Note> note;

	if (literal.starts_with("p")) {
		note = Note{};
	} else if (auto const base = note_index(literal[0]); base != u8(-1)) {
		note = Note { .base = base };

		while (literal.remove_prefix(1), not literal.empty()) {
			switch (literal.front()) {
			case '#': case 's': ++*note->base; break;
			case 'b': case 'f': --*note->base; break;
			default:
				goto endloop;
			}
		}
endloop:
		;
	}

	if (note && literal.size()) {
		auto &octave = note->octave.emplace();
		auto [p, ec] = std::from_chars(&literal.front(), 1 + &literal.back(), octave);
		ensure(p == &literal.back() + 1,          "Parser must have failed in recognizing what is considered a valid note token #1");
		ensure(ec != std::errc::invalid_argument, "Parser must have failed in recognizing what is considered a valid note token #2");
		ensure(ec != std::errc::result_out_of_range, "Octave number is too high"); // TODO(assert)
	}

	return note;
}

std::optional<u8> Note::into_midi_note() const
{
	return octave ? std::optional(into_midi_note(0)) : std::nullopt;
}

u8 Note::into_midi_note(i8 default_octave) const
{
	ensure(bool(this->base), "Pause don't translate into MIDI");
	auto const octave = this->octave.has_value() ? *this->octave : default_octave;
	// octave is in range [-1, 9] where Note { .base = 0, .octave = -1 } is midi note 0
	return (octave + 1) * 12 + *base;
}

void Note::simplify_inplace()
{
	if (base && octave) {
		octave = std::clamp(*octave + int(*base / 12), -1, 9);
		if ((*base %= 12) < 0) {
			base = 12 + *base;
		}
	}
}

std::partial_ordering Note::operator<=>(Note const& rhs) const
{
	if (base.has_value() == rhs.base.has_value()) {
		if (!base.has_value()) {
			if (length.has_value() == rhs.length.has_value() && length.has_value()) {
				return *length <=> *rhs.length;
			}
			return std::partial_ordering::unordered;
		}

		if (octave.has_value() == rhs.octave.has_value()) {
			if (octave.has_value())
				return (12 * *octave) + *base <=> (12 * *rhs.octave) + *rhs.base;
			return *base <=> *rhs.base;
		}
	}
	return std::partial_ordering::unordered;
}

std::ostream& operator<<(std::ostream& os, Note note)
{
	note.simplify_inplace();
	if (note.base) {
		os << note_index_to_string(*note.base);
		if (note.octave) {
			os << int(*note.octave);
		}
	} else {
		os << "p";
	}
	if (note.length) {
		os << " " << *note.length;
	}
	return os;
}

bool Note::operator==(Note const& other) const
{
	return octave == other.octave && base == other.base && length == other.length;
}

