#ifndef MUSIQUE_TRY_HH
#define MUSIQUE_TRY_HH

#include <musique/result.hh>

/// Shorthand for forwarding error values with Result type family.
///
/// This implementation requires C++ language extension: statement expressions
/// It's supported by GCC and Clang, other compilers i don't know.
/// Inspired by SerenityOS TRY macro
#define Try(Value)                                             \
	({                                                           \
		auto try_value = (Value);                                  \
	 	using Trait [[maybe_unused]] = Try_Traits<std::decay_t<decltype(try_value)>>; \
		if (not Trait::is_ok(try_value)) [[unlikely]]                \
			return Trait::yield_error(std::move(try_value));                \
		Trait::yield_value(std::move(try_value));                              \
	})

/// Abstraction over any value that are either value or error
///
/// Inspired by P2561R0
template<typename = void>
struct Try_Traits
{
	template<typename T>
	static constexpr bool is_ok(T const& v) { return Try_Traits<T>::is_ok(v); }

	template<typename T>
	static constexpr auto yield_value(T&& v) { return Try_Traits<T>::yield_value(std::forward<T>(v)); }

	template<typename T>
	static constexpr auto yield_error(T&& v) { return Try_Traits<T>::yield_error(std::forward<T>(v)); }
};

template<>
struct Try_Traits<std::optional<Error>>
{
	using Value_Type = std::nullopt_t;
	using Error_Type = Error;

	static constexpr bool is_ok(std::optional<Error> const& o)
	{
		return not o.has_value();
	}

	static std::nullopt_t yield_value(std::optional<Error>&& err)
	{
		assert(not err.has_value(), "Trying to yield value from optional that contains error");
		return std::nullopt;
	}

	static Error yield_error(std::optional<Error>&& err)
	{
		assert(err.has_value(), "Trying to yield value from optional that NOT constains error");
		return std::move(*err);
	}
};

template<typename T>
struct Try_Traits<Result<T>>
{
	using Value_Type = T;
	using Error_Type = Error;

	static constexpr bool is_ok(Result<T> const& o)
	{
		return o.has_value();
	}

	static auto yield_value(Result<T> val)
	{
		assert(val.has_value(), "Trying to yield value from expected that contains error");
		if constexpr (std::is_void_v<T>) {
		} else {
			return std::move(*val);
		}
	}

	static Error yield_error(Result<T>&& val)
	{
		assert(not val.has_value(), "Trying to yield error from expected with value");
		return std::move(val.error());
	}
};

#endif
