#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <edit_distance.hh>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <musique/bit_field.hh>
#include <musique/lexer/lines.hh>
#include <musique/pretty.hh>
#include <musique/runner.hh>
#include <musique/try.hh>
#include <musique/ui/program_arguments.hh>
#include <musique/unicode.hh>
#include <musique/user_directory.hh>
#include <span>
#include <thread>
#include <unordered_set>

#include <replxx.hxx>

#ifdef _WIN32
extern "C" {
#include <io.h>
}
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

bool tokens_only_mode = false;
bool ast_only_mode = false;
bool enable_repl = false;

// TODO: This variable is sus. It is used in a care-free manner and it usage should be reviewed
unsigned repl_line_number = 1;

#define Ignore(Call) do { auto const ignore_ ## __LINE__ = (Call); (void) ignore_ ## __LINE__; } while(0)

void print_repl_help()
{
	std::cout <<
		"List of all available commands of Musique interactive mode:\n"
		":exit - quit interactive mode\n"
		":help - prints this help message\n"
		":!<command> - allows for execution of any shell command\n"
		":clear - clears screen\n"
		":load <file> - loads file into Musique session\n"
		":ports - print list available ports"
		;
}

/// Trim spaces from left an right
static void trim(std::string_view &s)
{
	// left trim
	if (auto const i = std::find_if_not(s.begin(), s.end(), unicode::is_space); i != s.begin()) {
		// std::string_view::remove_prefix has UB when we wan't to remove more characters then str has
		// src: https://en.cppreference.com/w/cpp/string/basic_string_view/remove_prefix
		if (i != s.end()) {
			s.remove_prefix(std::distance(s.begin(), i));
		} else {
			s = {};
		}
	}

	// right trim
	if (auto const ws_end = std::find_if_not(s.rbegin(), s.rend(), unicode::is_space); ws_end != s.rbegin()) {
		s.remove_suffix(std::distance(ws_end.base(), s.end()));
	}
}

/// All source code through life of the program should stay allocated, since
/// some of the strings are only views into source
std::vector<std::string> eternal_sources;

/// Handles commands inside REPL session (those starting with ':')
///
/// Returns if one of command matched
static Result<bool> handle_repl_session_commands(std::string_view input, Runner &runner)
{
	using Handler = std::optional<Error>(*)(Runner &runner, std::optional<std::string_view> arg);
	using Command = std::pair<std::string_view, Handler>;

	static constexpr auto Commands = std::array {
		Command { "exit",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::exit(0);
			}
		},
		Command { "quit",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::exit(0);
			}
		},
		Command { "version",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::cout << Musique_Version << std::endl;
				return {};
			},
		},
		Command { "clear",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				std::cout << "\x1b[1;1H\x1b[2J" << std::flush;
				return {};
			}
		},
		Command { "help",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				print_repl_help();
				return {};
			}
		},
		Command { "load",
			+[](Runner& runner, std::optional<std::string_view> arg) -> std::optional<Error> {
				if (not arg.has_value()) {
					std::cerr << ":load subcommand requires path to file that will be loaded" << std::endl;
					return {};
				}
				auto path = *arg;
				std::ifstream source_file{std::string(path)};
				if (not source_file.is_open()) {
					std::cerr << ":load cannot find file " << path << std::endl;
					return {};
				}
				eternal_sources.emplace_back(std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>());
				Lines::the.add_file(std::string(path), eternal_sources.back());
				return runner.run(eternal_sources.back(), path);
			}
		},
		Command {
			"snap",
			+[](Runner &runner, std::optional<std::string_view> arg) -> std::optional<Error> {
				std::ostream *out = &std::cout;
				std::fstream file;
				if (arg.has_value() && arg->size()) {
					file.open(std::string(*arg));
				}
				runner.interpreter.snapshot(*out);
				return std::nullopt;
			}
		},

		Command {
			"ports",
			+[](Runner&, std::optional<std::string_view>) -> std::optional<Error> {
				midi::Rt_Midi{}.list_ports(std::cout);
				return {};
			},
		},

		Command {
			"time",
			+[](Runner& runner, std::optional<std::string_view> command) -> std::optional<Error> {
				std::optional<bool> enable = std::nullopt;
				if (command) {
					if (command == "enable") {
						enable = true;
					} else if (command == "disable") {
						enable = false;
					}
				}

				if (!enable.has_value()) {
					enable = !holds_alternative<Execution_Options::Time_Execution>(runner.default_options);
				}

				if (*enable) {
					std::cout << "Timing execution is on" << std::endl;
					runner.default_options |= Execution_Options::Time_Execution;
				} else {
					std::cout << "Timing execution is off" << std::endl;
					runner.default_options &= ~Execution_Options::Time_Execution;
				}
				return {};
			},
		},
	};

	if (input.starts_with('!')) {
		// TODO Maybe use different shell invoking mechanism then system
		// since on Windows it uses cmd.exe and not powershell which is
		// strictly superior
		Ignore(system(input.data() + 1));
		return true;
	}

	auto ws = input.find_first_of(" \t\n");
	auto provided_command = input.substr(0, input.find_first_of(" \t\n"));
	auto arg = ws == std::string_view::npos ? std::string_view{} : input.substr(ws);
	trim(arg);

	for (auto [command_name, handler] : Commands) {
		if (provided_command == command_name) {
			Try(handler(runner, arg));
			return true;
		}
	}

	return false;
}

static Runner *runner;

void sigint_handler(int sig)
{
	if (sig == SIGINT) {
		runner->interpreter.issue_interrupt();
	}
	std::signal(SIGINT, sigint_handler);
}

/// Fancy main that supports Result forwarding on error (Try macro)
[[maybe_unused]]
static std::optional<Error> Main(std::span<char const*> args)
{
	enable_repl = args.empty();

	// TODO: is_tty should be in ui namespace
	if (ui::program_arguments::is_tty() && getenv("NO_COLOR") == nullptr) {
		pretty::terminal_mode();
	}

	std::vector<ui::program_arguments::Run> runnables;

	while (args.size()) if (auto failed = ui::program_arguments::accept_commandline_argument(runnables, args)) {
		std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
		std::cerr << " Failed to recognize parameter " << std::quoted(*failed) << std::endl;
		ui::program_arguments::print_close_matches(args.front());
		std::exit(1);
	}

	Runner runner;
	::runner = &runner;
	std::signal(SIGINT, sigint_handler);

	for (auto const& [type, argument] : runnables) {
		if (type == ui::program_arguments::Run::Argument) {
			Lines::the.add_line("<arguments>", argument, repl_line_number);
			Try(runner.run(argument, "<arguments>"));
			repl_line_number++;
			continue;
		}
		auto path = argument;
		if (path == "-") {
			eternal_sources.emplace_back(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
		} else {
			if (not std::filesystem::exists(path)) {
				std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
				std::cerr << " couldn't open file: " << path << std::endl;
				std::exit(1);
			}
			std::ifstream source_file{std::filesystem::path(path)};
			eternal_sources.emplace_back(std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>());
		}

		Lines::the.add_file(std::string(path), eternal_sources.back());
		if (type == ui::program_arguments::Run::File) {
			Try(runner.run(eternal_sources.back(), path));
		} else {
			Try(runner.deffered_file(eternal_sources.back(), argument));
		}
	}

	enable_repl = enable_repl || (!runnables.empty() && std::all_of(runnables.begin(), runnables.end(),
		[](ui::program_arguments::Run const& run) { return run.type == ui::program_arguments::Run::Deffered_File; }));

	if (enable_repl) {
		repl_line_number = 1;


		replxx::Replxx repl;

		auto const history_path = (user_directory::data_home() / "history").string();

		repl.set_max_history_size(2048);
		repl.set_max_hint_rows(3);
		repl.history_load(history_path);

		for (;;) {
			char const* input = nullptr;
			do input = repl.input("> "); while((input == nullptr) && (errno == EAGAIN));

			if (input == nullptr) {
				break;
			}

			// Raw input line used for execution in language
			std::string_view raw = input;

			// Used to recognize REPL commands
			std::string_view command = raw;
			trim(command);

			if (command.empty()) {
				continue;
			}

			repl.history_add(std::string(command));
			repl.history_save(history_path);

			if (command.starts_with(':')) {
				command.remove_prefix(1);
				if (!Try(handle_repl_session_commands(command, runner))) {
					std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
					std::cerr << " unrecognized REPL command '" << command << '\'' << std::endl;
				}
				continue;
			}

			Lines::the.add_line("<repl>", raw, repl_line_number);
			auto result = runner.run(raw, "<repl>", Execution_Options::Print_Result);
			using Traits = Try_Traits<std::decay_t<decltype(result)>>;
			if (not Traits::is_ok(result)) {
				std::cout << std::flush;
				std::cerr << Traits::yield_error(std::move(result)) << std::flush;
			}
			repl_line_number++;
			// We don't free input line since there could be values that still relay on it
		}
	}

	return {};
}

#ifndef MUSIQUE_UNIT_TESTING

int main(int argc, char const** argv)
{
	auto const args = std::span(argv, argc).subspan(1);
	auto const result = Main(args);
	if (result.has_value()) {
		std::cerr << result.value() << std::flush;
		return 1;
	}
	return 0;
}

#endif // MUSIQUE_UNIT_TESTING
