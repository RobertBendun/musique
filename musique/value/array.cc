#include <musique/value/array.hh>
#include <musique/value/value.hh>

Array::Array()             = default;
Array::Array(Array const&) = default;
Array::Array(Array &&)     = default;
Array::~Array()            = default;

Array::Array(std::vector<Value>&& elements)
	: elements{std::move(elements)}
{
}

Result<Value> Array::index(Interpreter&, unsigned position) const
{
	if (elements.size() < position) {
		return errors::Out_Of_Range {
			.required_index = position,
			.size = elements.size()
		};
	}
	return elements[position];
}

usize Array::size() const
{
	return elements.size();
}

std::ostream& operator<<(std::ostream& os, Array const& v)
{
	os << '[';
	for (auto it = v.elements.begin(); it != v.elements.end(); ++it) {
		os << *it;
		if (std::next(it) != v.elements.end()) {
			os << "; ";
		}
	}
	return os << ']';
}
