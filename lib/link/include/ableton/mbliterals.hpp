#pragma once

#include <cstdint>
#include <iterator>
#include <numeric>
#include <concepts>

namespace ableton
{
	consteval std::int32_t bytes(std::convertible_to<char const*> auto const &array)
	{
		return std::accumulate(array, array + std::size(array) - 1, std::int32_t(0), [](auto p, auto c)
		{
			return (p << 8) | c;
		});
	}
}
