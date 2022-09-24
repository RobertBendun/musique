#include <musique/value/intrinsic.hh>
#include <musique/value/value.hh>

Result<Value> Intrinsic::operator()(Interpreter& interpreter, std::vector<Value> args) const
{
	return function_pointer(interpreter, std::move(args));
}
