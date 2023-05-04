#ifndef MUSIQUE_BIT_FIELD_HH
#define MUSIQUE_BIT_FIELD_HH

#include <type_traits>

template<typename>
struct Enable_Bit_Field_Operations : std::false_type
{
};

template<typename Enum>
concept Bit_Field = std::is_enum_v<Enum> and Enable_Bit_Field_Operations<Enum>::value;

template<Bit_Field T>
constexpr T operator|(T lhs, T rhs) noexcept
{
	return static_cast<T>(static_cast<std::underlying_type_t<T>>(lhs) | static_cast<std::underlying_type_t<T>>(rhs));
}

template<Bit_Field T>
constexpr T operator&(T lhs, T rhs) noexcept
{
	return static_cast<T>(static_cast<std::underlying_type_t<T>>(lhs) & static_cast<std::underlying_type_t<T>>(rhs));
}

template<Bit_Field auto V>
constexpr bool holds_alternative(decltype(V) e) noexcept
{
	return (e & V) == V;
}

#endif // MUSIQUE_BIT_FIELD_HH
