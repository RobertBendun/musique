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
		if (err.invalid_character) {
			return os << "unrecognized charater U+" << std::hex << err.invalid_character
				<< " (char: '" << utf8::Print{err.invalid_character} << "')\n";
		} else {
			return os << "unrecognized character\n";
		}
	}

	return os << "unrecognized error type\n";
}

Error errors::unrecognized_character(u32 invalid_character)
{
	Error err;
	err.type = errors::Unrecognized_Character;
	err.invalid_character = invalid_character;
	return err;
}

Error errors::unrecognized_character(u32 invalid_character, Location location)
{
	Error err;
	err.type = errors::Unrecognized_Character;
	err.invalid_character = invalid_character;
	err.location = std::move(location);
	return err;
}
