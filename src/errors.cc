#include <musique.hh>

bool Error::operator==(errors::Type type)
{
	return this->type == type;
}

std::ostream& operator<<(std::ostream& os, Error const&)
{
	return os << "generic error";
}
