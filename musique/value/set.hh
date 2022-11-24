#ifndef MUSIQUE_VALUE_SET_HH
#define MUSIQUE_VALUE_SET_HH

#include <unordered_set>
#include <utility>

// Needs to be always first, before all the implicit template instantiations
#include <musique/value/hash.hh>

#include <musique/value/collection.hh>
#include <musique/value/function.hh>
#include <musique/common.hh>

struct Value;

struct Set : Collection, Function
{
	std::unordered_set<Value> elements;

	~Set() = default;

	Result<Value> operator()(Interpreter &i, std::vector<Value> params) const override;

	Result<Value> index(Interpreter &i, unsigned position) const override;

	usize size() const override;

	bool is_collection() const override;

	bool operator==(Set const&) const = default;
	std::strong_ordering operator<=>(Set const&) const;
};

std::ostream& operator<<(std::ostream& out, Set const& set);

#endif
