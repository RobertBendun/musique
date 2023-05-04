#include <musique/value/number.hh>
#include <musique/try.hh>

#include <cmath>
#include <numeric>
#include <charconv>

Number::Number(value_type v)
	: num(v), den(1)
{
}

Number::Number(value_type num, value_type den)
	: num(num), den(den)
{
}

auto Number::as_int() const -> i64
{
	// We don't perform GCD simplification in place due to constness
	auto self = simplify();
	ensure(self.den == 1 || self.num == 0, "Implicit coarce to integer while holding fractional number is not allowed");
	return self.num;
}

auto Number::simplify() const -> Number
{
	auto copy = *this;
	copy.simplify_inplace();
	return copy;
}

// TODO This function may behave weirdly for maximum & minimum values of i64
void Number::simplify_inplace()
{
	for (;;) {
		if (auto d = std::gcd(num, den); d != 1) {
			num /= d;
			den /= d;
		} else {
			break;
		}
	}

	if (den < 0) {
		// When num < 0 yields num > 0
		// When num > 0 yields num < 0
		den = -den;
		num = -num;
	}
}

bool Number::operator==(Number const& rhs) const
{
	return (*this <=> rhs) == 0;
}

bool Number::operator!=(Number const& rhs) const
{
	return !(*this == rhs);
}

std::strong_ordering Number::operator<=>(Number const& rhs) const
{
	return (num * rhs.den - den * rhs.num) <=> 0;
}

Number Number::operator+(Number const& rhs) const
{
	int dens_lcm = std::lcm(den, rhs.den);
  	return Number{(num * (dens_lcm / den)) + (rhs.num * (dens_lcm / rhs.den)), dens_lcm}.simplify();
}

Number& Number::operator+=(Number const& rhs)
{
	int dens_lcm = std::lcm(den, rhs.den);
   	num = (num * (dens_lcm / den)) + (rhs.num * (dens_lcm / rhs.den));
   	den = dens_lcm;
   	simplify_inplace();
   	return *this;
}

Number Number::operator-(Number const& rhs) const
{
	int dens_lcm = std::lcm(den, rhs.den);
  	return Number{(num * (dens_lcm / den)) - (rhs.num * (dens_lcm / rhs.den)), dens_lcm}.simplify();
}

Number& Number::operator-=(Number const& rhs)
{
	int dens_lcm = std::lcm(den, rhs.den);
   	num = (num * (dens_lcm / den)) - (rhs.num * (dens_lcm / rhs.den));
   	den = dens_lcm;
   	simplify_inplace();
   	return *this;
}

Number Number::operator*(Number const& rhs) const
{
  	return Number{num * rhs.num, den * rhs.den}.simplify();
}

Number& Number::operator*=(Number const& rhs)
{
   	num *= rhs.num;
   	den *= rhs.den;
   	simplify_inplace();
   	return *this;
}

Result<Number> Number::operator/(Number const& rhs) const
{
	if (rhs.num == 0) {
		return Error {
			.details = errors::Arithmetic {
				.type = errors::Arithmetic::Division_By_Zero
			}
		};
	}
	return Number{num * rhs.den, den * rhs.num}.simplify();
}

inline auto modular_inverse(Number::value_type a, Number::value_type n)
	-> Result<Number::value_type>
{
	using N = Number::value_type;
	N t = 0, newt = 1;
	N r = n, newr = a;

	while (newr != 0) {
		auto q = r / newr;
		t = std::exchange(newt, t - q * newt);
		r = std::exchange(newr, r - q * newr);
	}

	if (r > 1) {
		return Error {
			.details = errors::Arithmetic {
				.type = errors::Arithmetic::Unable_To_Calculate_Modular_Multiplicative_Inverse
			}
		};
	}

	return t < 0 ? t + n : t;
}


Result<Number> Number::operator%(Number const& rhs) const
{
	if (rhs.num == 0) {
		return Error {
			.details = errors::Arithmetic {
				.type = errors::Arithmetic::Division_By_Zero
			}
		};
	}

	auto ret = simplify();
	auto [rnum, rden] = rhs.simplify();

	if (rden != 1) {
		return Error {
			.details = errors::Arithmetic {
				.type = errors::Arithmetic::Fractional_Modulo
			}
		};
	}

	if (ret.den == 1) {
		ret.num %= rnum;
		return ret;
	}

	return Number(
		(Try(modular_inverse(ret.den, rnum)) * ret.num) % rnum
	);
}

std::ostream& operator<<(std::ostream& os, Number const& n)
{
	return n.den == 1
		? os << n.num
		: os << n.num << '/' << n.den;
}

static constexpr usize Number_Of_Powers = std::numeric_limits<Number::value_type>::digits10 + 1;

/// Computes compile time array with all possible powers of `multiplier` for given `Number::value_type` value range.
static consteval auto compute_powers(Number::value_type multiplier) -> std::array<Number::value_type, Number_Of_Powers>
{
	using V = Number::value_type;

	std::array<V, Number_Of_Powers> powers;
	powers.front() = 1;
	std::transform(
		powers.begin(), std::prev(powers.end()),
		std::next(powers.begin()),
		[multiplier](V x) { return x * multiplier; });
	return powers;
}

/// Returns `10^n`
static Number::value_type pow10(usize n)
{
	static constexpr auto Powers = compute_powers(10);
	ensure(n < Powers.size(), "Trying to compute power of 10 grater then current type can hold");
	return Powers[n];
}

Result<Number> Number::from(Token token)
{
	Number result;

	bool parsed_numerator = false;
	auto const begin = token.source.data();
	auto const end   = token.source.data() + token.source.size();

	auto [num_end, ec] = std::from_chars(begin, end, result.num);

	if (ec != std::errc{}) {
		if (ec == std::errc::invalid_argument && begin != end && *begin == '.') {
			num_end = begin;
			goto parse_fractional;
		}
		return Error {
			.details = errors::Failed_Numeric_Parsing { .reason = ec },
			.location = std::move(token.location)
		};
	}
	parsed_numerator = true;

	if (num_end != end && *num_end == '.') {
parse_fractional:
		++num_end;
		decltype(Number::num) frac;
		auto [frac_end, ec] = std::from_chars(num_end, end, frac);
		if (ec != std::errc{}) {
			if (parsed_numerator && ec == std::errc::invalid_argument && token.source != ".")
				return result;
			return Error {
				.details = errors::Failed_Numeric_Parsing { .reason = ec },
				.location = std::move(token.location)
			};
		}
		result += Number{ (result.num < 0 ? -1 : 1) * frac, pow10(frac_end - num_end) };
	}

	return result.simplify();
}

namespace impl
{
	enum class Rounding_Mode { Ceil, Floor, Round };

	static Number round(Number result, Rounding_Mode rm)
	{
		if (result.den == -1 || result.den == 1) {
			return result.simplify();
		}
		auto const negative = (result.num < 0) xor (result.den < 0);
		if (result.num < 0) result.num *= -1;
		if (result.den < 0) result.den *= -1;

		if (auto const r = result.num % result.den; r != 0) {
			bool ceil = rm == Rounding_Mode::Ceil;
			ceil |= rm == Rounding_Mode::Round && (negative
				? r*2 <= result.den
				: r*2 >= result.den);

			if (ceil ^ negative) {
				result.num += result.den;
			}

			// C++ integer division handles floor case
			result.num /= result.den;
			result.den = 1;
		} else {
			result.simplify_inplace();
		}
		result.num *= negative ? -1 : 1;
		return result;
	}
}

Number Number::floor() const
{
	return impl::round(*this, impl::Rounding_Mode::Floor);
}

Number Number::ceil()  const
{
	return impl::round(*this, impl::Rounding_Mode::Ceil);
}

Number Number::round() const
{
	return impl::round(*this, impl::Rounding_Mode::Round);
}

Result<Number> Number::inverse() const
{
	if (num == 0) {
		return Error {
			.details = errors::Arithmetic {
				.type = errors::Arithmetic::Division_By_Zero
			}
		};
	}
	return { den, num };
}

namespace impl
{
	// Raise Number to integer power helper
	static inline Result<Number> pow(Number const& x, decltype(Number::num) n)
	{
		// We simply raise numerator and denominator to required power
		// and if n is negative we take inverse.
		if (n == 0) return Number(1);
		auto result = Number { 1, 1 };

		auto flip = false;
		if (n < 0) { flip = true; n = -n; }

		for (auto i = n; i != 0; --i) result.num *= x.num;
		for (auto i = n; i != 0; --i) result.den *= x.den;
		return flip ? Try(result.inverse()) : result;
	}
}

Result<Number> Number::pow(Number n) const
{
	n.simplify_inplace();

	// Simple case, we raise this to integer power.
	if (n.den == 1) {
		return impl::pow(*this, n.num);
	}

	// Hard case, we raise this to fractional power.
	// Essentialy finding n.den root of (x to n.num power).
	// We need to protect ourselfs against even roots of negative numbers.
	unimplemented("nth root calculation is not implemented yet");
}

std::size_t std::hash<Number>::operator()(Number const& value) const
{
	std::hash<Number::value_type> h;
	return hash_combine(h(value.num), h(value.den));
}

#ifdef MUSIQUE_UNIT_TESTING

#include <catch_amalgamated.hpp>

TEST_CASE("Number arithmetic operators", "[number]")
{
	REQUIRE(Number{1, 8} + Number{3, 4} == Number{ 7,  8});
	REQUIRE(Number{1, 8} - Number{3, 4} == Number{-5,  8});
	REQUIRE(Number{1, 8} * Number{3, 4} == Number{ 3, 32});
	REQUIRE(Number{1, 8} / Number{3, 4} == Number{ 1,  6});
}


TEST_CASE("Number::floor()", "[number]")
{
	REQUIRE(Number(0)  == Number(1, 2).floor());
	REQUIRE(Number(1)  == Number(3, 2).floor());
	REQUIRE(Number(-1) == Number(-1, 2).floor());
	REQUIRE(Number(0)  == Number(0).floor());
	REQUIRE(Number(1)  == Number(1).floor());
	REQUIRE(Number(-1) == Number(-1).floor());
}

TEST_CASE("Number::ceil()", "[number]")
{
	REQUIRE(Number(1)  == Number(1, 2).ceil());
	REQUIRE(Number(2)  == Number(3, 2).ceil());
	REQUIRE(Number(0)  == Number(-1, 2).ceil());
	REQUIRE(Number(-1) == Number(-3, 2).ceil());
	REQUIRE(Number(0)  == Number(0).ceil());
	REQUIRE(Number(1)  == Number(1).ceil());
	REQUIRE(Number(-1) == Number(-1).ceil());
}

TEST_CASE("Number::round()", "[number]")
{
	REQUIRE(Number(1) == Number(3, 4).round());
	REQUIRE(Number(0) == Number(1, 4).round());
	REQUIRE(Number(1) == Number(5, 4).round());
	REQUIRE(Number(2) == Number(7, 4).round());

	REQUIRE(Number(-1) == Number(-3, 4).round());
	REQUIRE(Number(0)  == Number(-1, 4).round());
	REQUIRE(Number(-1) == Number(-5, 4).round());
	REQUIRE(Number(-2) == Number(-7, 4).round());
};

#endif
