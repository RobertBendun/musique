#include <musique.hh>

#include <iostream>

Interpreter::Interpreter()
	: Interpreter(std::cout)
{
}

Interpreter::Interpreter(std::ostream& out)
	: out(out)
{
	functions["typeof"] = [](std::vector<Value> args) -> Value {
		assert(args.size() == 1, "typeof expects only one argument");
		switch (args.front().type) {
		case Value::Type::Nil:    return Value::symbol("nil");
		case Value::Type::Number: return Value::symbol("number");
		case Value::Type::Symbol: return Value::symbol("symbol");
		}
		unreachable();
	};

	functions["say"] = [&](std::vector<Value> args) -> Value {
		for (auto it = args.begin(); it != args.end(); ++it) {
			out << *it;
			if (std::next(it) != args.end())
				out << ' ';
		}
		out << '\n';
		return {};
	};
}

Result<Value> Interpreter::eval(Ast &&ast)
{
	switch (ast.type) {
	case Ast::Type::Literal:
		return Value::from(ast.token);

	case Ast::Type::Binary:
		{
			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : ast.arguments) {
				values.push_back(Try(eval(std::move(a))));
			}

			Value result = values.front();
			if (ast.token.source == "+") {
				for (auto &v : std::span(values).subspan(1)) {
					assert(result.type == Value::Type::Number, "LHS of + should be a number");
					assert(v.type == Value::Type::Number,      "RHS of + should be a number");
					result.n += v.n;
				}
				return result;
			} else if (ast.token.source == "*") {
				for (auto &v : std::span(values).subspan(1)) {
					assert(result.type == Value::Type::Number, "LHS of * should be a number");
					assert(v.type == Value::Type::Number,      "RHS of * should be a number");
					result.n *= v.n;
				}
				return result;
			}

			unimplemented();
		}
		break;

	case Ast::Type::Sequence:
		{
			Value v;
			for (auto &a : ast.arguments)
				v = Try(eval(std::move(a)));
			return v;
		}

	case Ast::Type::Call:
		{
			Value func = Try(eval(std::move(ast.arguments.front())));
			assert(func.type == Value::Type::Symbol, "Currently only symbols can be called");
			if (auto it = functions.find(func.s); it != functions.end()) {
				std::vector<Value> values;
				values.reserve(ast.arguments.size());
				for (auto& a : std::span(ast.arguments).subspan(1)) {
					values.push_back(Try(eval(std::move(a))));
				}
				return it->second(std::move(values));
			} else {
				return errors::function_not_defined(func);
			}
		}

	default:
		unimplemented();
	}
}
