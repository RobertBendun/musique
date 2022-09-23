#ifndef MUSIQUE_FORMAT_HH
#define MUSIQUE_FORMAT_HH

#include <musique/result.hh>

struct Interpreter;
struct Value;

struct Value_Formatter
{
	enum Context
	{
		Free,
		Inside_Block
	};

	Context context = Free;
	unsigned indent = 0;

	Value_Formatter nest(Context nested = Free) const;

	std::optional<Error> format(std::ostream& os, Interpreter &interpreter, Value const& value);
};

Result<std::string> format(Interpreter &i, Value const& value);

#endif
