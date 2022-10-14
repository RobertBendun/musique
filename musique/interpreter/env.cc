#include <musique/interpreter/env.hh>

#include <iostream>

std::shared_ptr<Env> Env::global = nullptr;

std::shared_ptr<Env> Env::make()
{
	auto new_env = new Env();
	ensure(new_env, "Cannot construct new env");
	return std::shared_ptr<Env>(new_env);
}

Env& Env::force_define(std::string name, Value new_value)
{
	variables.insert_or_assign(std::move(name), std::move(new_value));
	return *this;
}

Value* Env::find(std::string const& name)
{
	for (Env *env = this; env; env = env->parent.get()) {
		if (auto it = env->variables.find(name); it != env->variables.end()) {
			return &it->second;
		}
	}
	return nullptr;
}

std::shared_ptr<Env> Env::enter()
{
	auto next = make();
	next->parent = shared_from_this();
	return next;
}

std::shared_ptr<Env> Env::leave()
{
	return parent;
}
