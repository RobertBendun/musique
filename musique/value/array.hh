#ifndef MUSIQUE_ARRAY_HH
#define MUSIQUE_ARRAY_HH

#include <musique/result.hh>
#include <musique/value/collection.hh>
#include <vector>

struct Interpreter;
struct Value;

// TODO Array should have an ability to be invoked with duration parameter like singular note

/// Eager Array
struct Array : Collection
{
	/// Elements that are stored in array
	std::vector<Value> elements;

	Array();
	explicit Array(std::vector<Value>&&);
	Array(Array const&);
	Array(Array &&);
	~Array() override;

	Array& operator=(Array const&) = default;
	Array& operator=(Array &&) = default;

	/// Index element of an array
	Result<Value> index(Interpreter &i, unsigned position) const override;

	/// Count of elements
	usize size() const override;

	/// Arrays are equal if all of their elements are equal
	bool operator==(Array const&) const = default;

	bool is_collection() const override;

	/// Print array
	friend std::ostream& operator<<(std::ostream& os, Array const& v);
};


#endif
