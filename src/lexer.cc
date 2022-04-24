#include "musique.hh"

auto Lexer::next_token() -> Result<Token>
{
	return {};
}

std::ostream& operator<<(std::ostream& os, Token const&)
{
	os << "Token";
	return os;
}
