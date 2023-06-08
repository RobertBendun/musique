#ifndef MUSIQUE_RANDOM_HH
#define MUSIQUE_RANDOM_HH

#include <compare>
#include <concepts>
#include <musique/errors.hh>
#include <random>

/// Random number distributions that behave the same across all supported platforms
///
/// C++ standard defines how random number generators should behave exactly.
/// It DOES NOT define how distributions (and functions using distributions like std::shuffle)
/// should behave so we need to provide deterministic implementation for all platforms by ourselfs
namespace musique::random // TODO Adopt this musique prefix elsewere?
{
	// TODO: Add other distributions that may be useful in musical context (and expose them to the user of the language)

	/// uniform(g, a, b) is portable std::uniform_int_distribution{a,b}(g)
	template<typename Int = int, std::uniform_random_bit_generator Generator>
	constexpr Int uniform(Generator &generator, Int a, Int b) noexcept(noexcept(generator()))
	{
		ensure(a <= b, "uniform(gen, a, b) requires a < b");

		// Inspired by algorithm used in libstdc++-v3/include/bits/uniform_int_dist.h

		using result_type = typename Generator::result_type;
		using common_type = std::common_type_t<result_type, std::make_unsigned_t<result_type>>;

		constexpr common_type generator_min = Generator::min();
		constexpr common_type generator_max = Generator::max();
		static_assert(generator_min < generator_max, "Uniform random bit generator must define min() < max()");

		constexpr common_type generator_range = generator_max - generator_min;
		common_type const distribution_range = common_type(b) - common_type(a);

		common_type result;

		// switch(generator_range <=> distribution_range) would be nice, however it's not supported :<
		if (generator_range == distribution_range) {
			// ranges are equal, trivial case
			return generator() - generator_min + a;
		} else if (generator_range < distribution_range) {
			// We need to upscale generated range to distribution range by splitting distribution_range into parts equal to generator_range.
			// Then we choose one of the splits randomly, and inside it we choose one of the numbers.
			// If we chosen something that is not inside distribution range then we repeat the process
			// TODO: Why additional check result < tmp?
			common_type tmp;
			do
			{
				const common_type splits = generator_range + 1;
				tmp = (splits * uniform<Int>(generator, 0, distribution_range / splits));
				result = tmp + (common_type(generator()) - generator_min);
			}
			while (result > distribution_range || result < tmp);
			return result + a;
		} else {
			// We need to downscale generated range to distribution range by splitting generator range into chunks of distribution range
			// If generated number exceeds chunks then we repeat until we find one that does.
			// Then we can simply descale from chunks to numbers.
			common_type const effective_range = distribution_range + 1; // distribution_range can be zero
			common_type const scaling = generator_range / effective_range;
			common_type const end = effective_range * scaling;
			do result = common_type(generator()) - generator_min; while (result >= end);
			return result / scaling + a;
		}
	}

	template<std::random_access_iterator It, typename Generator>
	requires std::uniform_random_bit_generator<std::decay_t<Generator>>
	void shuffle(It first, It last, Generator &&generator)
	{
		for (std::iter_difference_t<It> i = last - first - 1; i > 0; --i) {
			using std::swap;
			swap(first[i], first[uniform(generator, std::iter_difference_t<It>(0), i)]);
		}
	}

}

#endif
