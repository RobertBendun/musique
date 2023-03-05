// Copyright 2023 Robert Bendun <robert@bendun.cc>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <numeric>
#include <ranges>
#include <type_traits>
#include <vector>

template<std::random_access_iterator S, std::random_access_iterator T>
requires std::equality_comparable_with<
	std::iter_value_t<S>,
	std::iter_value_t<T>
>
constexpr int edit_distance(S s, unsigned m, T t, unsigned n)
{
	std::array<std::vector<int>, 2> memo;
	auto *v0 = &memo[0];
	auto *v1 = &memo[1];

	for (auto& v : memo) {
		v.resize(n+1);
	}
	std::iota(v0->begin(), v0->end(), 0);

	for (auto i = 0u; i < m; ++i) {
		(*v1)[0] = i+1;
		for (auto j = 0u; j < n; ++j) {
			auto const deletion_cost = (*v0)[j+1] + 1;
			auto const insertion_cost = (*v1)[j] + 1;
			auto const substitution_cost = (*v0)[j] + (s[i] != t[j]);

			(*v1)[j+1] = std::min({ deletion_cost, insertion_cost, substitution_cost });
		}
		std::swap(v0, v1);
	}

	return (*v0)[n];
}

template<
	std::ranges::random_access_range Range1,
	std::ranges::random_access_range Range2
>
requires std::equality_comparable_with<
	std::ranges::range_value_t<Range1>,
	std::ranges::range_value_t<Range2>
>
constexpr int edit_distance(Range1 const& range1, Range2 const& range2)
{
	return edit_distance(
		std::begin(range1), std::ranges::size(range1),
		std::begin(range2), std::ranges::size(range2)
	);
}
