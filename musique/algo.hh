#ifndef MUSIQUE_ALGO_HH
#define MUSIQUE_ALGO_HH

#include <musique/result.hh>
#include <musique/try.hh>
#include <musique/value/value.hh>
#include <ranges>
#include <compare>

/// Generic algorithms support
namespace algo
{
	/// Check if predicate is true for all successive pairs of elements
	constexpr bool pairwise_all(
		auto &&range,
		auto &&binary_predicate)
	{
		auto it = std::begin(range);
		auto const end_it = std::end(range);
		for (auto next_it = std::next(it); it != end_it && next_it != end_it; ++it, ++next_it) {
			if (not binary_predicate(*it, *next_it)) {
				return false;
			}
		}
		return true;
	}

	/// Fold that stops iteration on error value via Result type
	template<typename T>
	constexpr Result<T> fold(auto&& range, T init, auto &&reducer)
		requires (is_template_v<Result, decltype(reducer(std::move(init), *range.begin()))>)
	{
		for (auto &&value : range) {
			init = Try(reducer(std::move(init), value));
		}
		return init;
	}

	/// Equavilent of std::lexicographical_compare_three_way
	///
	/// It's missing from MacOS C++ standard library
	constexpr auto lexicographical_compare(auto&& lhs_range, auto&& rhs_range)
	{
		auto lhs = lhs_range.begin();
		auto rhs = rhs_range.begin();
		decltype(*lhs <=> *rhs) result = std::partial_ordering::equivalent;
		for (; lhs != lhs_range.end() && rhs != rhs_range.end(); ++lhs, ++rhs) {
			if ((result = *lhs <=> *rhs) != 0) {
				return result;
			}
		}
		return result;
	}
}

/// Flattens one layer: `[[[1], 2], 3]` becomes `[[1], 2, 3]`
Result<std::vector<Value>> flatten(Interpreter &i, std::span<Value>);
Result<std::vector<Value>> flatten(Interpreter &i, std::vector<Value>);

#endif
