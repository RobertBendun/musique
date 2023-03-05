#ifndef MUSIQUE_CMD_HH
#define MUSIQUE_CMD_HH

#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace cmd
{
	/// Describes all arguments that will be run
	struct Run
	{
		enum Type
		{
			File,
			Argument,
			Deffered_File
		} type;

		std::string_view argument;
	};

	/// Accept and execute next command line argument with its parameters if it has any
	std::optional<std::string_view> accept_commandline_argument(std::vector<cmd::Run> &runnables, std::span<char const*> &args);

	/// Print all arguments that are similar to one provided
	void print_close_matches(std::string_view arg);

	/// Recognize if stdout is connected to terminal
	bool is_tty();

	[[noreturn]]
	void usage();
}

#endif // MUSIQUE_CMD_HH

