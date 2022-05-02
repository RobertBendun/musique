#include <musique.hh>

bool Error::operator==(errors::Type type)
{
	return this->type == type;
}

std::ostream& operator<<(std::ostream& os, Error const& err)
{
	if (err.location) {
		os << *err.location;
	} else {
		os << "musique";
	}

	os << ": error: ";

	switch (err.type) {
	case errors::End_Of_File:
		return os << "end of file\n";

	case errors::Unrecognized_Character:
		return os << "unrecognized charater 0x" << std::hex << err.invalid_character
			<< "(char: '" << utf8::Print(err.invalid_character) << "')\n";
	}

	return os << "unrecognized error type\n";
}
