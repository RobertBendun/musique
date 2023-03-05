#ifndef MUSIQUE_RESULT_HH
#define MUSIQUE_RESULT_HH

#include <tl/expected.hpp>

#include <musique/errors.hh>

/// Holds either T or Error
template<typename T>
struct [[nodiscard("This value may contain critical error, so it should NOT be ignored")]] Result : tl::expected<T, Error>
{
	using Storage = tl::expected<T, Error>;

	constexpr Result() = default;

	template<typename ...Args> requires (not std::is_void_v<T>) && std::is_constructible_v<T, Args...>
	constexpr Result(Args&& ...args)
		: Storage( T{ std::forward<Args>(args)... } )
	{
	}

	template<typename Arg> requires std::is_constructible_v<Storage, Arg>
	constexpr Result(Arg &&arg)
		: Storage(std::forward<Arg>(arg))
	{
	}

	inline Result(Error error)
		: Storage(tl::unexpected(std::move(error)))
	{
	}

	template<typename Arg>
	requires requires (Arg a) {
		{ Error { .details = std::move(a) } };
	}
	inline Result(Arg a)
		: Storage(tl::unexpected(Error { .details = std::move(a) }))
	{
	}

	// Internal function used for definition of Try macro
	inline auto value() &&
	{
		if constexpr (not std::is_void_v<T>) {
			// NOTE This line in ideal world should be `return Storage::value()`
			// but C++ does not infer that this is rvalue context.
			// `std::add_rvalue_reference_t<Storage>::value()`
			// also does not work, so this is probably the best way to express this:
			return std::move(*static_cast<Storage*>(this)).value();
		}
	}

	/// Fill error location if it's empty and we have an error
	inline Result<T> with(File_Range file) &&
	{
		if (!Storage::has_value()) {
			if (auto& target = Storage::error().file; !target || target == File_Range{}) {
				target = file;
			}
		}
		return *this;
	}

	inline tl::expected<T, Error> to_expected() &&
	{
		return *static_cast<Storage*>(this);
	}

	template<typename Map>
	requires is_template_v<Result, std::invoke_result_t<Map, T&&>>
	auto and_then(Map &&map) &&
	{
		return std::move(*static_cast<Storage*>(this)).and_then(
			[map = std::forward<Map>(map)](T &&value) {
				return std::move(map)(std::move(value)).to_expected();
			});
	}

	using Storage::and_then;

	operator std::optional<Error>() &&
	{
		return Storage::has_value() ? std::nullopt : std::optional(Storage::error());
	}
};

#endif
