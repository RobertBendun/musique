#ifndef MUSIQUE_NUMBER_HH
#define MUSIQUE_NUMBER_HH

#include <musique/common.hh>
#include <musique/result.hh>
#include <musique/token.hh>
#include <ostream>

/// Number type supporting integer and fractional constants
///
/// \invariant gcd(num, den) == 1, after any operation
struct Number
{
	/// Type that represents numerator and denominator values
	using value_type = i64;

	value_type num = 0; ///< Numerator of a fraction beeing represented
	value_type den = 1; ///< Denominator of a fraction beeing represented

	constexpr Number()              = default;
	constexpr Number(Number const&) = default;
	constexpr Number(Number &&)     = default;
	constexpr Number& operator=(Number const&) = default;
	constexpr Number& operator=(Number &&)     = default;

	explicit Number(value_type v);          ///< Creates Number as fraction v / 1
	Number(value_type num, value_type den); ///< Creates Number as fraction num / den

	auto as_int()      const -> value_type; ///< Returns self as int
	auto simplify()    const -> Number;     ///< Returns self, but with gcd(num, den) == 1
	void simplify_inplace();                ///< Update self, to have gcd(num, den) == 1

	bool operator==(Number const&) const;
	bool operator!=(Number const&) const;
	std::strong_ordering operator<=>(Number const&) const;

	Number  operator+(Number const& rhs) const;
	Number& operator+=(Number const& rhs);
	Number  operator-(Number const& rhs) const;
	Number& operator-=(Number const& rhs);
	Number  operator*(Number const& rhs) const;
	Number& operator*=(Number const& rhs);
	Result<Number> operator/(Number const& rhs) const;
	Result<Number> operator%(Number const& rhs) const;

	Number floor() const; ///< Return number rounded down to nearest integer
	Number ceil()  const; ///< Return number rounded up to nearest integer
	Number round() const; ///< Return number rounded to nearest integer

	Result<Number> inverse()     const; ///< Return number raised to power -1
	Result<Number> pow(Number n) const; ///< Return number raised to power `n`.

	/// Parses source contained by token into a Number instance
	static Result<Number> from(Token token);
};

std::ostream& operator<<(std::ostream& os, Number const& num);

template<> struct std::hash<Number> { std::size_t operator()(Number const&) const; };

#endif
