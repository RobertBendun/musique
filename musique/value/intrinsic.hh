#ifndef MUSIQUE_VALUE_INTRINSIC_HH
#define MUSIQUE_VALUE_INTRINSIC_HH

#include <musique/result.hh>
#include <musique/value/function.hh>

struct Interpreter;
struct Value;

struct Intrinsic : Function
{
	using Function_Pointer = Result<Value>(*)(Interpreter &i, std::vector<Value>);
	Function_Pointer function_pointer = nullptr;

	constexpr Intrinsic() = default;

	constexpr Intrinsic(Function_Pointer fp)
		: function_pointer{fp}
	{
	}

	constexpr ~Intrinsic() = default;

	/// Calls underlying function pointer
	Result<Value> operator()(Interpreter&, std::vector<Value>) const override;

	/// Compares if function pointers are equal
	bool operator==(Intrinsic const&) const = default;
};

#endif // MUSIQUE_VALUE_INTRINSIC_HH
