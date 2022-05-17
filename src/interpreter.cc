#include <musique.hh>

#include <iostream>

static auto numeric_binary_operator(auto binop)
{
	return [binop = std::move(binop)](std::vector<Value> args) -> Result<Value> {
		auto result = std::move(args.front());
		for (auto &v : std::span(args).subspan(1)) {
			assert(result.type == Value::Type::Number, "LHS should be a number");
			assert(v.type == Value::Type::Number,      "RHS should be a number");
			result.n = binop(std::move(result.n), v.n);
		}
		return result;
	};
}

Interpreter::Interpreter()
	: Interpreter(std::cout)
{
}

Interpreter::~Interpreter()
{
	Env::pool = nullptr;
}

Interpreter::Interpreter(std::ostream& out)
	: out(out)
{
	assert(Env::pool == nullptr, "Only one instance of interpreter can be at one time");
	Env::pool = &env_pool;

	auto &global = env_pool.emplace_back();
	global.parent_enviroment_id = 0;

	global.force_define("typeof", [](std::vector<Value> args) -> Value {
		assert(args.size() == 1, "typeof expects only one argument");
		return Value::symbol(std::string(type_name(args.front().type)));
	});

	global.force_define("say", [&out](std::vector<Value> args) -> Value {
		for (auto it = args.begin(); it != args.end(); ++it) {
			out << *it;
			if (std::next(it) != args.end())
				out << ' ';
		}
		out << '\n';
		return {};
	});

	operators["+"] = numeric_binary_operator(std::plus<>{});
	operators["-"] = numeric_binary_operator(std::minus<>{});
	operators["*"] = numeric_binary_operator(std::multiplies<>{});
	operators["/"] = numeric_binary_operator(std::divides<>{});
}

Env& Interpreter::env()
{
	return env_pool[current_env];
}

Env const& Interpreter::env() const
{
	return env_pool[current_env];
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

			auto op = operators.find(std::string(ast.token.source));

			if (op == operators.end()) {
				return errors::unresolved_operator(ast.token);
			}

			for (auto& a : ast.arguments) {
				values.push_back(Try(eval(std::move(a))));
			}

			return op->second(std::move(values));
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
			auto location = ast.arguments.front().location;
			Value func = Try(eval(std::move(ast.arguments.front())));
			if (func.type != Value::Type::Symbol)
				return errors::not_callable(std::move(location), func.type);

			if (auto body = env().find(func.s); body) {
				std::vector<Value> values;
				values.reserve(ast.arguments.size());
				for (auto& a : std::span(ast.arguments).subspan(1)) {
					values.push_back(Try(eval(std::move(a))));
				}
				return (*body)(std::move(values));
			} else {
				return errors::function_not_defined(func);
			}
		}

	default:
		unimplemented();
	}
}
