#include <sstream>
#include <musique/interpreter/env.hh>
#include <musique/format.hh>
#include <musique/try.hh>
#include <musique/value/value.hh>

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
	return std::visit(Overloaded {
		[&](Intrinsic const& intrinsic) -> std::optional<Error> {
			for (auto const& [key, val] : Env::global->variables) {
				if (auto other = get_if<Intrinsic>(val); intrinsic == *other) {
					os << "<intrinsic '" << key << "'>";
					return {};
				}
			}
			os << "<intrinsic>";
			return {};
		},
		[&](Array const& array) -> std::optional<Error> {
			os << '(';
			for (auto i = 0u; i < array.elements.size(); ++i) {
				if (i > 0) {
					os << ", ";
				}
				Try(nest(Inside_Block).format(os, interpreter, array.elements[i]));
			}
			os << ')';
			return {};
		},
		[&](Block const& block) -> std::optional<Error> {
			os << '(';
			for (auto i = 0u; i < block.size(); ++i) {
				if (i > 0) {
					os << ", ";
				}
				Try(nest(Inside_Block).format(os, interpreter, Try(block.index(interpreter, i))));
			}
			os << ')';
			return {};
		},
		[&](auto&&) -> std::optional<Error> {
			os << value;
			return {};
		}
	}, value.data);
}
