#ifndef MUSIQUE_VALUE_COLLECTION_HH
#define MUSIQUE_VALUE_COLLECTION_HH

#include <musique/result.hh>

struct Interpreter;
struct Value;

/// Abstraction of Collection
struct Collection
{
	virtual ~Collection() = default;

	/// Return element at position inside collection
	virtual Result<Value> index(Interpreter &i, unsigned position) const = 0;

	/// Return elements count
	virtual usize size() const = 0;

	bool operator==(Collection const&) const = default;
};

#endif // MUSIQUE_VALUE_COLLECTION_HH
