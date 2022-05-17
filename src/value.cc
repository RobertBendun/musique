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
	v.n = std::move(n).simplify();
	return v;
}

Value Value::symbol(std::string s)
{
	Value v;
	v.type = Type::Symbol;
	v.s = std::move(s);
	return v;
}

Value Value::lambda(Function f)
{
	Value v;
	v.type = Type::Lambda;
	v.f = std::move(f);
	return v;
}

Result<Value> Value::operator()(std::vector<Value> args)
{
	if (type == Type::Lambda) {
		return f(std::move(args));
	}
	// TODO Fill location
	return errors::not_callable(std::nullopt, type);
}

bool Value::operator==(Value const& other) const
{
	if (type != other.type)
		return false;

	switch (type) {
	case Type::Nil:    return true;
	case Type::Number: return n == other.n;
	case Type::Symbol: return s == other.s;
	case Type::Lambda: return false; // TODO Reconsider if functions are comparable
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

	case Value::Type::Lambda:
		return os << "<lambda>";
	}
	unreachable();
}

std::string_view type_name(Value::Type t)
{
	switch (t) {
	case Value::Type::Lambda: return "lambda";
	case Value::Type::Nil:    return "nil";
	case Value::Type::Number: return "number";
	case Value::Type::Symbol: return "symbol";
	}
	unreachable();
}
