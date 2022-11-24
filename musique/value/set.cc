#include <musique/value/value.hh>
#include <musique/value/set.hh>


Result<Value> Set::operator()(Interpreter&, std::vector<Value> params) const
{
	auto copy = *this;

	if (auto a = match<Number>(params)) {
		auto [length] = *a;
		for (auto& value : copy.elements) {
			if (auto chord = get_if<Chord>(value)) {
				// FIXME This const_cast should be unnesesary
				for (auto &note : const_cast<Chord*>(chord)->notes) {
					note.length = length;
				}
				continue;
			}
		}
	}

	// TODO Error reporting when parameters doesnt match
	// TODO reconsider different options for this data structure invocation
	return copy;
}

Result<Value> Set::index(Interpreter&, unsigned position) const
{
	if (elements.size() < position) {
		return errors::Out_Of_Range {
			.required_index = position,
			.size = elements.size()
		};
	}
	// FIXME std::unordered_set::iterator has forward iterator, which means
	// that any element lookup has complecity O(n). This may have serious
	// performance implications and further investigation is needed.
	return *std::next(elements.begin(), position);
}

usize Set::size() const
{
	return elements.size();
}

bool Set::is_collection() const
{
	return true;
}

std::strong_ordering Set::operator<=>(Set const&) const
{
	unimplemented();
}

std::ostream& operator<<(std::ostream& out, Set const& set)
{
	unimplemented();
	return out;
}
