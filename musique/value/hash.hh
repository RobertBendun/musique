#ifndef MUSIQUE_VALUE_HASH_HH
#define MUSIQUE_VALUE_HASH_HH

#include <functional>

#define Hash_For(Name) \
	struct Name; \
	template<> \
	struct std::hash<Name> { \
		size_t operator()(Name const&) const noexcept; \
	}; \

Hash_For(Block)
Hash_For(Set)
Hash_For(Value)

#endif // MUSIQUE_VALUE_HASH_HH
