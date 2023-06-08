#include <musique/random.hh>

#ifdef MUSIQUE_UNIT_TESTING

// TODO: Don't include whole catch in each translation unit
#include <catch_amalgamated.hpp>

TEST_CASE("Deterministic uniform rng", "[random]")
{
	std::mt19937 rnd{42};

	// Pregenerated sequence with the same seed as in rnd
	static constexpr int expected_sequence[] = {
		4, 8, 10, 2, 8, 8, 6, 6, 1, 4, 1, 1, 0, 5, 9, 3,
		6, 1, 7, 7, 0, 0, 10, 7, 9, 10, 2, 0, 2, 10, 2, 6,
		3, 6, 5, 0, 4, 0, 3, 5, 6, 4, 1, 0, 3, 10, 4, 2,
		5, 0, 8, 6, 2, 4, 5, 10, 6, 5, 0, 9, 6, 7, 1, 4
	};

	for (auto i = 0u; i < std::size(expected_sequence); ++i) {
		auto const generated = musique::random::uniform(rnd, 0, 10);

		DYNAMIC_SECTION("Uniform generation #" << i) {
			REQUIRE(expected_sequence[i] == generated);
		}
		// Don't test more then we need
		if (expected_sequence[i] != generated)
			break;
	}
}

#endif
