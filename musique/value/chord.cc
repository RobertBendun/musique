#include <musique/accessors.hh>
#include <musique/guard.hh>
#include <musique/try.hh>
#include <musique/value/chord.hh>
#include <musique/value/value.hh>

Chord::Chord(Note note)
	: notes{ note }
{
}

Chord::Chord(std::vector<Note> &&notes)
	: notes{std::move(notes)}
{
}

Chord Chord::from(std::string_view source)
{
	auto note = Note::from(source);
	ensure(note.has_value(), "don't know how this could happen");

	Chord chord;
	chord.notes.push_back(*std::move(note));
	return chord;
}

Result<Value> Chord::operator()(Interpreter& interpreter, std::vector<Value> args) const
{
	std::vector<Value> array;
	std::vector<Chord> current = { *this };

	enum State {
		Waiting_For_Length,
		Waiting_For_Note
	} state = Waiting_For_Length;

	static constexpr auto guard = Guard<1> {
		.name = "note creation",
		.possibilities = {
			"(note:music [duration:number])+"
		}
	};

	auto const next = [&state] {
		switch (state) {
		break; case Waiting_For_Length: state = Waiting_For_Note;
		break; case Waiting_For_Note:   state = Waiting_For_Length;
		}
	};

	auto const update = [&state](Chord &chord, Number number) -> std::optional<Error> {
		auto const resolve = [&chord](auto field, auto new_value) {
			for (auto &note : chord.notes) {
				(note.*field) = new_value;
			}
		};

		switch (state) {
		break; case Waiting_For_Length:
			resolve(&Note::length, number);
			return {};

		default:
			return guard.yield_error();
		}
	};

	for (auto &arg : args) {
		if (auto collection = get_if<Collection>(arg)) {
			if (state != Waiting_For_Length) {
				return guard.yield_error();
			}

			auto const ring_size = current.size();
			for (usize i = 0; i < arg.size() && current.size() < arg.size(); ++i) {
				current.push_back(current[i % ring_size]);
			}

			for (usize i = 0; i < current.size(); ++i) {
				Value value = Try(collection->index(interpreter, i % collection->size()));
				if (auto number = get_if<Number>(value)) {
					Try(update(current[i], *number));
					continue;
				}
			}
			next();
			continue;
		}

		if (auto number = get_if<Number>(arg)) {
			for (auto &chord : current) {
				Try(update(chord, *number));
			}
			next();
			continue;
		}

		if (auto chord = get_if<Chord>(arg)) {
			std::transform(current.begin(), current.end(), std::back_inserter(array),
				[](Chord &c) { return std::move(c); });
			current.clear();
			current.push_back(std::move(*chord));
			state = Waiting_For_Length;
		}
	}

	std::move(current.begin(), current.end(), std::back_inserter(array));

	ensure(not array.empty(), "At least *this should be in this array");
	if (array.size() == 1) {
		return array.front();
	}
	return array;
}

std::ostream& operator<<(std::ostream& os, Chord const& chord)
{
	if (chord.notes.size() == 1) {
		return os << chord.notes.front();
	}

	os << "chord (";
	for (auto it = chord.notes.begin(); it != chord.notes.end(); ++it) {
		os << *it;
		if (std::next(it) != chord.notes.end())
			os << "; ";
	}
	return os << ')';
}
