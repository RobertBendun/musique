#ifndef MUSIQUE_ARRAY_HH
#define MUSIQUE_ARRAY_HH

#include <vector>
#include <musique/result.hh>

struct Interpreter;
struct Value;

/// Eager Array
struct Array
{
	/// Elements that are stored in array
	std::vector<Value> elements;

	/// Index element of an array
	Result<Value> index(Interpreter &i, unsigned position) const;

	/// Count of elements
	usize size() const;

	bool operator==(Array const&) const = default;
};

std::ostream& operator<<(std::ostream& os, Array const& v);

#endif
