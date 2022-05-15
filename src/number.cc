#include <musique.hh>
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
	assert(self.den == 1 || self.num == 0, "Implicit coarce to integer while holding fractional number is not allowed");
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

Number Number::operator/(Number const& rhs) const
{
  	return Number{num * rhs.den, den * rhs.num}.simplify();
}

Number& Number::operator/=(Number const& rhs)
{
   	num *= rhs.den;
   	den *= rhs.num;
   	simplify_inplace();
   	return *this;
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
	assert(n < Powers.size(), "Trying to compute power of 10 grater then current type can hold");
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
		return errors::failed_numeric_parsing(std::move(token.location), ec, token.source);
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
			return errors::failed_numeric_parsing(std::move(token.location), ec, token.source);
		}
		result += Number{ frac, pow10(frac_end - num_end) };
	}

	return result.simplify();
}
