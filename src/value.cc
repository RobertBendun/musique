#include <musique.hh>

Result<Value> Value::from(Token t)
{
	switch (t.type) {
	case Token::Type::Numeric:
		return Number::from(std::move(t)).map(Value::number);

	case Token::Type::Symbol:
		if (t.source == "nil")
			return Value{};
		return Value::symbol(std::string(t.source));

	default:
		unimplemented();
	}
}

Value Value::number(Number n)
{
	Value v;
	v.type = Type::Number;
	v.n = n;
	return v;
}

Value Value::symbol(std::string s)
{
	Value v;
	v.type = Type::Symbol;
	v.s = s;
	return v;
}

bool Value::operator==(Value const& other) const
{
	if (type != other.type)
		return false;

	switch (type) {
	case Type::Nil:    return true;
	case Type::Number: return n == other.n;
	case Type::Symbol: return s == other.s;
	}

	unreachable();
}

std::ostream& operator<<(std::ostream& os, Value const& v)
{
	switch (v.type) {
	case Value::Type::Nil:
		return os << "nil";

	case Value::Type::Number:
		return os << v.n;

	case Value::Type::Symbol:
		return os << v.s;
	}
	unreachable();
}
