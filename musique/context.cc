#include <musique/context.hh>

Note Context::fill(Note note) const
{
	if (not note.octave) note.octave = octave;
	if (not note.length) note.length = length;
	return note;
}

std::chrono::duration<float> Context::length_to_duration(std::optional<Number> length) const
{
	auto const len = length ? *length : this->length;
	return std::chrono::duration<float>(float(len.num * (60.f / (float(bpm) / 4))) / len.den);
}
