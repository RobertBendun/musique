#include <musique.hh>
#include <cmath>
#include <numeric>

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
