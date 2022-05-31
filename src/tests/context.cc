#include <boost/ut.hpp>
#include <musique.hh>

using namespace boost::ut;
using namespace std::string_view_literals;

void test_seconds_compute(
	unsigned bpm,
	Number length,
	float value,
	reflection::source_location const& sl = reflection::source_location::current())
{
	Context const ctx { .bpm = bpm };
	auto const dur = ctx.length_to_duration(length);

	static_assert(std::same_as<decltype(dur)::period, std::ratio<1, 1>>,
		"tests provided by this function expects Context::length_to_duration to return seconds");

	expect(dur.count() == _f(value), sl);
}

suite context_suite = [] {
	"Context"_test = [] {
		should("resolve note duration length to seconds") = [] {
			test_seconds_compute(60, Number(2,1), 8);
			test_seconds_compute(60, Number(1,1), 4);
			test_seconds_compute(60, Number(1,2), 2);
			test_seconds_compute(60, Number(1,4), 1);

			test_seconds_compute(96, Number(2,1), 5);
			test_seconds_compute(96, Number(1,1), 2.5);
			test_seconds_compute(96, Number(1,2), 1.25);
			test_seconds_compute(96, Number(1,4), 0.625);

			test_seconds_compute(120, Number(2,1), 4);
			test_seconds_compute(120, Number(1,1), 2);
			test_seconds_compute(120, Number(1,2), 1);
			test_seconds_compute(120, Number(1,4), 0.5);
		};

		should("fill notes with default context values") = [] {
			Context ctx;
			ctx.length = Number(1, 42);
			ctx.octave = 8;

			expect(eq(ctx.fill({ .base = 0                                   }), Note { .base = 0, .octave = 8, .length = Number(1, 42) }));
			expect(eq(ctx.fill({ .base = 0, .length = Number(4)              }), Note { .base = 0, .octave = 8, .length = Number(4) }));
			expect(eq(ctx.fill({ .base = 0, .octave = 1                      }), Note { .base = 0, .octave = 1, .length = Number(1, 42) }));
			expect(eq(ctx.fill({ .base = 0, .octave = 1, .length = Number(4) }), Note { .base = 0, .octave = 1, .length = Number(4) }));
		};
	};
};
