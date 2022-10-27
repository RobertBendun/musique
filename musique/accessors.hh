#ifndef MUSIQUE_ACCESSORS_HH
#define MUSIQUE_ACCESSORS_HH

#include <musique/common.hh>
#include <musique/errors.hh>
#include <variant>
#include <iostream>
#include <musique/value/collection.hh>

namespace details
{
	template<typename T>
	concept Reference = std::is_lvalue_reference_v<T>;
}

template<typename Desired, same_template_as<std::variant> Variant>
requires details::Reference<Variant>
constexpr auto get_if(Variant &&v)
{
	using Return_Type = std::conditional_t<
		std::is_const_v<std::remove_reference_t<Variant>>,
		Desired const*,
		Desired*
	>;

	return std::visit([]<details::Reference Actual>(Actual&& act) -> Return_Type {
		if constexpr (std::is_same_v<Desired, std::remove_cvref_t<Actual>>) {
			return &act;
		} else if constexpr (std::is_base_of_v<Desired, std::remove_cvref_t<Actual>>) {
			auto ret = static_cast<Return_Type>(&act);
			if constexpr (std::is_same_v<std::remove_cvref_t<Desired>, Collection>) {
				if (ret->is_collection()) {
					return ret;
				} else {
					return nullptr;
				}
			} else {
				return ret;
			}
		}
		return nullptr;
	}, v);
}

template<typename Desired, typename ...V>
constexpr Desired& get_ref(std::variant<V...> &v)
{
	if (auto result = get_if<Desired>(v)) { return *result; }
	unreachable();
}

#endif

