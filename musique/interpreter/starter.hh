#ifndef MUSIQUE_STARTER_HH
#define MUSIQUE_STARTER_HH

#include <memory>

struct Starter
{
	Starter();
	void start();
	void stop();

	size_t peers() const;

	struct Implementation;
	std::shared_ptr<Implementation> impl;
};


#endif // MUSIQUE_STARTER_HH
