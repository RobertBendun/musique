#include <musique.hh>

#include <iostream>

static auto binary_operator(auto binop)
{
	return [binop = std::move(binop)](Interpreter&, std::vector<Value> args) -> Result<Value> {
		auto result = std::move(args.front());
		for (auto &v : std::span(args).subspan(1)) {
			assert(result.type == Value::Type::Number, "LHS should be a number");
			assert(v.type == Value::Type::Number,      "RHS should be a number");
			if constexpr (std::is_same_v<Number, std::invoke_result_t<decltype(binop), Number, Number>>) {
				result.n = binop(std::move(result.n), std::move(v).n);
			} else {
				result.type = Value::Type::Bool;
				result.b = binop(std::move(result.n), std::move(v).n);
			}
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
	Env::global.reset();
}

Interpreter::Interpreter(std::ostream& out)
	: out(out)
{
	assert(!bool(Env::global), "Only one instance of interpreter can be at one time");

	env = Env::global = Env::make();
	auto &global = *Env::global;

	global.force_define("typeof", [](Interpreter&, std::vector<Value> args) -> Value {
		assert(args.size() == 1, "typeof expects only one argument");
		return Value::symbol(std::string(type_name(args.front().type)));
	});

	global.force_define("if", [](Interpreter &i, std::vector<Value> args) -> Result<Value> {
		assert(args.size() == 2 || args.size() == 3, "argument count does not add up - expected: if <condition> <then> [<else>]");
		if (args.front().truthy()) {
			return args[1](i, {});
		} else if (args.size() == 3) {
			return args[2](i, {});
		} else {
			return Value{};
		}
	});

	global.force_define("say", [](Interpreter &i, std::vector<Value> args) -> Value {
		for (auto it = args.begin(); it != args.end(); ++it) {
			i.out << *it;
			if (std::next(it) != args.end())
				i.out << ' ';
		}
		i.out << '\n';
		return {};
	});

	operators["+"] = binary_operator(std::plus<>{});
	operators["-"] = binary_operator(std::minus<>{});
	operators["*"] = binary_operator(std::multiplies<>{});
	operators["/"] = binary_operator(std::divides<>{});

	operators["<"]  = binary_operator(std::less<>{});
	operators[">"]  = binary_operator(std::greater<>{});
	operators["<="] = binary_operator(std::less_equal<>{});
	operators[">="] = binary_operator(std::greater_equal<>{});
	operators["=="] = binary_operator(std::equal_to<>{});
	operators["!="] = binary_operator(std::not_equal_to<>{});
}

Result<Value> Interpreter::eval(Ast &&ast)
{
	switch (ast.type) {
	case Ast::Type::Literal:
		switch (ast.token.type) {
		case Token::Type::Symbol:
			if (ast.token.source != "nil") {
				auto const value = env->find(std::string(ast.token.source));
				assert(value, "Missing variable error is not implemented yet: variable: "s + std::string(ast.token.source));
				return *value;
			}
			return Value{};

		default:
			return Value::from(ast.token);
		}

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

			return op->second(*this, std::move(values));
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

			std::vector<Value> values;
			values.reserve(ast.arguments.size());
			for (auto& a : std::span(ast.arguments).subspan(1)) {
				values.push_back(Try(eval(std::move(a))));
			}
			return std::move(func)(*this, std::move(values));
		}

	case Ast::Type::Variable_Declaration:
		{
			assert(ast.arguments.size() == 2, "Only simple assigments are supported now");
			assert(ast.arguments.front().type == Ast::Type::Literal, "Only names are supported as LHS arguments now");
			assert(ast.arguments.front().token.type == Token::Type::Symbol, "Only names are supported as LHS arguments now");
			env->force_define(std::string(ast.arguments.front().token.source), Try(eval(std::move(ast.arguments.back()))));
			return Value{};
		}

	case Ast::Type::Lambda:
		{
			Lambda lambda;
			auto parameters = std::span(ast.arguments.begin(), std::prev(ast.arguments.end()));
			lambda.parameters.reserve(parameters.size());
			for (auto &param : parameters) {
				assert(param.type == Ast::Type::Literal && param.token.type == Token::Type::Symbol, "Not a name in parameter section of Ast::lambda");
				lambda.parameters.push_back(std::string(std::move(param).token.source));
			}

			lambda.context = env;
			lambda.body = std::move(ast.arguments.back());
			return Value::lambda(std::move(lambda));
		}

	default:
		unimplemented();
	}
}
void Interpreter::enter_scope()
{
	env = env->enter();
}

void Interpreter::leave_scope()
{
	assert(env != Env::global, "Cannot leave global scope");
	env = env->leave();
}
