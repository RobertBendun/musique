#ifndef MUSIQUE_VALUE_FUNCTION_HH
#define MUSIQUE_VALUE_FUNCTION_HH

#include <musique/result.hh>

struct Value;
struct Interpreter;

struct Function
{
	constexpr virtual ~Function() = default;
	virtual Result<Value> operator()(Interpreter &i, std::vector<Value> params) const = 0;

	constexpr bool operator==(Function const&) const = default;
};

#endif // MUSIQUE_VALUE_FUNCTION_HH
