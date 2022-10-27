#ifndef MUSIQUE_COMMON_HH
#define MUSIQUE_COMMON_HH

#include <cstdint>
#include <string>
#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;
using uint = unsigned int;

/// Combine several lambdas into one for visiting std::variant
template<typename ...Lambdas>
struct Overloaded : Lambdas... { using Lambdas::operator()...; };
template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

/// Returns if provided thingy is a given template
template<template<typename ...> typename Template, typename>
struct is_template : std::false_type {};

template<template<typename ...> typename Template, typename ...T>
struct is_template<Template, Template<T...>> : std::true_type {};

/// Returns if provided thingy is a given template
template<template<typename ...> typename Template, typename T>
constexpr auto is_template_v = is_template<Template, T>::value;

template<typename T, template<typename ...> typename Template>
concept same_template_as = is_template_v<Template, std::remove_cvref_t<T>>;

/// Drop in replacement for bool when C++ impilcit conversions stand in your way
struct Explicit_Bool
{
	bool value;
	constexpr Explicit_Bool(bool b) : value(b) { }
	constexpr Explicit_Bool(auto &&) = delete;
	constexpr operator bool() const { return value; }
};

constexpr std::size_t hash_combine(std::size_t lhs, std::size_t rhs) {
	return lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
}

template<typename>
static constexpr bool always_false = false;

template<typename T>
concept Three_Way_Comparable = requires (T const& lhs, T const& rhs) {
	{ lhs <=> rhs };
};

#endif
