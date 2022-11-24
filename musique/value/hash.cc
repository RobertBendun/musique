#include <musique/value/hash.hh>
#include <musique/value/value.hh>

size_t std::hash<Set>::operator()(Set const& set) const noexcept
{
	return std::accumulate(
		set.elements.cbegin(), set.elements.cend(),
		size_t(0),
		[](size_t hash, Value const& element) {
			return hash_combine(hash, std::hash<Value>{}(element));
		}
	);
}

std::size_t std::hash<Value>::operator()(Value const& value) const noexcept
{
	auto const value_hash = std::visit(Overloaded {
		[](Nil) {
			return std::size_t(0);
		},

		[](Intrinsic i) {
			return size_t(i.function_pointer);
		},

		[](Block const& b) {
			return hash_combine(std::hash<Ast>{}(b.body), b.parameters.size());
		},

		[this](Array const& array) {
			return std::accumulate(
				array.elements.begin(), array.elements.end(), size_t(0),
				[this](size_t h, Value const& v) { return hash_combine(h, operator()(v)); }
			);
		},

		[](Chord const& chord) {
			return std::accumulate(chord.notes.begin(), chord.notes.end(), size_t(0), [](size_t h, Note const& n) {
				h = hash_combine(h, std::hash<std::optional<int>>{}(n.base));
				h = hash_combine(h, std::hash<std::optional<Number>>{}(n.length));
				h = hash_combine(h, std::hash<std::optional<i8>>{}(n.octave));
				return h;
			});
		},

		[]<typename T>(T const& t) { return std::hash<T>{}(t); },
	}, value.data);

	return hash_combine(value_hash, size_t(value.data.index()));
}
