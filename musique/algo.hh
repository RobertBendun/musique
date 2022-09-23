#ifndef MUSIQUE_ALGO_HH
#define MUSIQUE_ALGO_HH

#include <musique/result.hh>
#include <musique/try.hh>
#include <musique/value/value.hh>
#include <ranges>

/// Generic algorithms support
namespace algo
{
	/// Check if predicate is true for all successive pairs of elements
	constexpr bool pairwise_all(
		std::ranges::forward_range auto &&range,
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
}

/// Flattens one layer: `[[[1], 2], 3]` becomes `[[1], 2, 3]`
Result<std::vector<Value>> flatten(Interpreter &i, std::span<Value>);
Result<std::vector<Value>> flatten(Interpreter &i, std::vector<Value>);

#endif
