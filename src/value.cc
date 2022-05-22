#include <musique.hh>

Result<Value> Value::from(Token t)
{
	switch (t.type) {
	case Token::Type::Numeric:
		return Value::number(Try(Number::from(std::move(t))));

	case Token::Type::Keyword:
		if (t.source == "false") return Value::boolean(false);
		if (t.source == "nil")   return Value{};
		if (t.source == "true")  return Value::boolean(true);
		unreachable();

	case Token::Type::Symbol:
		return Value::symbol(std::string(t.source));

	default:
		unimplemented();
	}
}

Value Value::boolean(bool b)
{
	Value v;
	v.type = Value::Type::Bool;
	v.b = b;
	return v;
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

Result<Value> Value::operator()(Interpreter &i, std::vector<Value> args)
{
	if (type == Type::Lambda) {
		return f(i, std::move(args));
	}
	// TODO Fill location
	return errors::not_callable(std::nullopt, type);
}

bool Value::truthy() const
{
	switch (type) {
	case Type::Bool:   return b;
	case Type::Nil:    return false;
	case Type::Number: return n != Number(0);
	case Type::Lambda:
	case Type::Symbol: return true;
	}
	unreachable();
}

bool Value::falsy() const
{
	return not truthy();
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
	case Type::Bool:   return b == other.b;
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

	case Value::Type::Bool:
		return os << (v.b ? "true" : "false");

	case Value::Type::Lambda:
		return os << "<lambda>";
	}
	unreachable();
}

std::string_view type_name(Value::Type t)
{
	switch (t) {
	case Value::Type::Bool:   return "bool";
	case Value::Type::Lambda: return "lambda";
	case Value::Type::Nil:    return "nil";
	case Value::Type::Number: return "number";
	case Value::Type::Symbol: return "symbol";
	}
	unreachable();
}

Result<Value> Lambda::operator()(Interpreter &i, std::vector<Value> arguments)
{
	auto old_scope = std::exchange(i.env, context);
	i.enter_scope();

	assert(parameters.size() == arguments.size(), "wrong number of arguments");

	for (usize j = 0; j < parameters.size(); ++j) {
		i.env->force_define(parameters[j], std::move(arguments[j]));
	}

	Ast body_copy = body;
	auto result = i.eval(std::move(body_copy));

	i.env = old_scope;
	return result;
}
