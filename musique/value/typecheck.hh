#ifndef MUSIQUE_TYPECHECK_HH
#define MUSIQUE_TYPECHECK_HH

#include <musique/value/value.hh>

/// Intrinsic implementation primitive providing a short way to check if arguments match required type signature
static inline bool typecheck(std::vector<Value> const& args, auto const& ...expected_types)
{
	return (args.size() == sizeof...(expected_types)) &&
		[&args, expected_types...]<std::size_t ...I>(std::index_sequence<I...>) {
			return ((expected_types == args[I].type) && ...);
		} (std::make_index_sequence<sizeof...(expected_types)>{});
}

/// Intrinsic implementation primitive providing a short way to move values based on matched type signature
static inline bool typecheck_front(std::vector<Value> const& args, auto const& ...expected_types)
{
	return (args.size() >= sizeof...(expected_types)) &&
		[&args, expected_types...]<std::size_t ...I>(std::index_sequence<I...>) {
			return ((expected_types == args[I].type) && ...);
		} (std::make_index_sequence<sizeof...(expected_types)>{});
}

/// Intrinsic implementation primitive providing a short way to move values based on matched type signature
template<auto ...Types>
static inline auto move_from(std::vector<Value>& args)
{
	return [&args]<std::size_t ...I>(std::index_sequence<I...>) {
		return std::tuple { (std::move(args[I]).*(Member_For_Value_Type<Types>::value)) ... };
	} (std::make_index_sequence<sizeof...(Types)>{});
}

/// Shape abstraction to define what types are required once
template<auto ...Types>
struct Shape
{
	static inline auto move_from(std::vector<Value>& args)       { return ::move_from<Types...>(args); }
	static inline auto typecheck(std::vector<Value>& args)       { return ::typecheck(args, Types...); }
	static inline auto typecheck_front(std::vector<Value>& args) { return ::typecheck_front(args, Types...); }

	static inline auto typecheck_and_move(std::vector<Value>& args)
	{
		return typecheck(args) ? std::optional { move_from(args) } : std::nullopt;
	}
};

#endif
