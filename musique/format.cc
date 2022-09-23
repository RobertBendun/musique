#include <musique/interpreter/env.hh>
#include <musique/format.hh>
#include <musique/try.hh>
#include <musique/value/value.hh>
#include <sstream>

Result<std::string> format(Interpreter &i, Value const& value)
{
	std::stringstream ss;
	Try(Value_Formatter{}.format(ss, i, value));
	return ss.str();
}

Value_Formatter Value_Formatter::nest(Context nested) const
{
	return Value_Formatter { .context = nested, .indent = indent + 2 };
}

std::optional<Error> Value_Formatter::format(std::ostream& os, Interpreter &interpreter, Value const& value)
{
	switch (value.type) {
	break; case Value::Type::Nil:
		os << "nil";

	break; case Value::Type::Symbol:
		os << value.s;

	break; case Value::Type::Bool:
		os << std::boolalpha << value.b;

	break; case Value::Type::Number:
		if (auto n = value.n.simplify(); n.den == 1) {
			os << n.num << '/' << n.den;
		} else {
			os << n.num;
		}

	break; case Value::Type::Intrinsic:
		for (auto const& [key, val] : Env::global->variables) {
			if (val.type == Value::Type::Intrinsic && val.intr == value.intr) {
				os << "<intrinsic '" << key << "'>";
				return {};
			}
		}
		os << "<intrinsic>";


	break; case Value::Type::Array:
		os << '[';
		for (auto i = 0u; i < value.array.elements.size(); ++i) {
			if (i > 0) {
				os << "; ";
			}
			Try(nest(Inside_Block).format(os, interpreter, value.array.elements[i]));
		}
		os << ']';

	break; case Value::Type::Block:
		os << '[';
		for (auto i = 0u; i < value.blk.size(); ++i) {
			if (i > 0) {
				os << "; ";
			}
			Try(nest(Inside_Block).format(os, interpreter, Try(value.index(interpreter, i))));
		}
		os << ']';

	break; case Value::Type::Music:
		os << value.chord;
	}

	return {};
}
