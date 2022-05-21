#include <musique.hh>

#include <iostream>

std::vector<Env> *Env::pool = nullptr;

Env& Env::force_define(std::string name, Value new_value)
{
	variables.insert_or_assign(std::move(name), std::move(new_value));
	return *this;
}

Env& Env::parent()
{
	return (*pool)[parent_enviroment_id];
}

Value* Env::find(std::string const& name)
{
	for (Env *prev = nullptr, *env = this; env != prev; prev = std::exchange(env, &env->parent())) {
		if (auto it = env->variables.find(name); it != env->variables.end()) {
			return &it->second;
		}
	}
	return nullptr;
}

usize Env::operator++() const
{
	auto const parent_id = this - pool->data();
	auto const free = std::find_if(pool->begin(), pool->end(), [](Env const& env) { return env.parent_enviroment_id == Env::Unused; });
	Env* next = free == pool->end()
		? &pool->emplace_back()
		: &*free;

	next->parent_enviroment_id = parent_id;
	return next - pool->data();
}

usize Env::operator--()
{
	if (this == &Env::global())
		return 0;
	variables.clear();
	return std::exchange(parent_enviroment_id, Unused);
}

Env& Env::global()
{
	return (*pool)[0];
}
