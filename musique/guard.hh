#ifndef MUSIQUE_GUARD_HH
#define MUSIQUE_GUARD_HH

#include <algorithm>
#include <array>
#include <musique/accessors.hh>
#include <musique/common.hh>
#include <musique/errors.hh>
#include <musique/value/value.hh>

/// Allows creation of guards that ensure proper type
template<usize N>
struct Guard
{
	std::string_view name;
	std::array<std::string_view, N> possibilities;
	errors::Unsupported_Types_For::Type type = errors::Unsupported_Types_For::Function;

	inline Error yield_error() const
	{
		auto error = errors::Unsupported_Types_For {
			.type = type,
			.name = std::string(name),
			.possibilities = {}
		};
		std::transform(possibilities.begin(), possibilities.end(), std::back_inserter(error.possibilities), [](auto s) {
			return std::string(s);
		});
		return Error { std::move(error) };
	}

	inline std::optional<Error> yield_result() const
	{
		return yield_error();
	}

	template<typename T>
	inline Result<T*> match(Value &v) const
	{
		if (auto p = get_if<T>(v)) {
			return p;
		} else {
			return yield_error();
		}
	}

	inline std::optional<Error> operator()(bool(*predicate)(Value const&), Value const& v) const
	{
		return predicate(v) ? std::optional<Error>{} : yield_result();
	}
};

#endif
