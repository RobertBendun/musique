#ifndef MUSIQUE_ACCESSORS_HH
#define MUSIQUE_ACCESSORS_HH

#include <musique/errors.hh>
#include <variant>

template<typename Desired, typename ...V>
constexpr Desired* get_if(std::variant<V...> &v)
{
	return std::visit([]<typename Actual>(Actual &act) -> Desired* {
		if constexpr (std::is_same_v<Desired, Actual>) {
			return &act;
		} else if constexpr (std::is_base_of_v<Desired, Actual>) {
			return static_cast<Desired*>(&act);
		} else {
			return nullptr;
		}
	}, v);
}

template<typename Desired, typename ...V>
constexpr Desired const* get_if(std::variant<V...> const& v)
{
	return std::visit([]<typename Actual>(Actual const& act) -> Desired const* {
		if constexpr (std::is_same_v<Desired, Actual>) {
			return &act;
		} else if constexpr (std::is_base_of_v<Desired, Actual>) {
			return static_cast<Desired const*>(&act);
		} else {
			return nullptr;
		}
	}, v);
}

template<typename Desired, typename ...V>
constexpr Desired& get_ref(std::variant<V...> &v)
{
	if (auto result = get_if<Desired>(v)) { return *result; }
	unreachable();
}

#if 0
template<typename ...T, typename Values>
constexpr auto match(Values& values) -> std::optional<std::tuple<T...>>
{
	return [&]<std::size_t ...I>(std::index_sequence<I...>) -> std::optional<std::tuple<T...>> {
		if (sizeof...(T) == values.size() && (std::holds_alternative<T>(values[I].data) && ...)) {
			return {{ std::get<T>(values[I].data)... }};
		} else {
			return std::nullopt;
		}
	} (std::make_index_sequence<sizeof...(T)>{});
}

template<typename ...T, typename Values>
constexpr auto match_ref(Values& values) -> std::optional<std::tuple<T&...>>
{
	return [&]<std::size_t ...I>(std::index_sequence<I...>) -> std::optional<std::tuple<T&...>> {
		if (sizeof...(T) == values.size() && (get_if<T>(values[I].data) && ...)) {
			return {{ get_ref<T>(values[I].data)... }};
		} else {
			return std::nullopt;
		}
	} (std::make_index_sequence<sizeof...(T)>{});
}
#endif

#endif

