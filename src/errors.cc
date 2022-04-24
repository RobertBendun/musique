#include "musique.hh"

std::ostream& operator<<(std::ostream& os, Error const&)
{
	return os << "generic error";
}
