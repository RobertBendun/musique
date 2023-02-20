#include <algorithm>
#include <array>
#include <edit_distance.hh>
#include <iomanip>
#include <iostream>
#include <limits>
#include <musique/cmd.hh>
#include <musique/common.hh>
#include <musique/interpreter/builtin_function_documentation.hh>
#include <set>
#include <unordered_set>
#include <variant>

#ifdef _WIN32
extern "C" {
#include <io.h>
}
#else
#include <unistd.h>
#endif

using Empty_Argument    = void(*)();
using Requires_Argument = void(*)(std::string_view);
using Defines_Code      = cmd::Run(*)(std::string_view);
using Parameter         = std::variant<Empty_Argument, Requires_Argument, Defines_Code>;

using namespace cmd;

// from musique/main.cc:
extern bool enable_repl;
extern bool ast_only_mode;
extern void usage();

static constexpr std::array all_parameters = [] {
	Defines_Code provide_function = [](std::string_view fname) -> cmd::Run {
		return { .type = Run::Deffered_File, .argument = fname };
	};

	Defines_Code provide_inline_code = [](std::string_view code) -> cmd::Run {
		return { .type = Run::Argument, .argument = code };
	};

	Defines_Code provide_file = [](std::string_view fname) -> cmd::Run {
		return { .type = Run::File, .argument = fname };
	};


	Requires_Argument show_docs = [](std::string_view builtin) {
		if (auto maybe_docs = find_documentation_for_builtin(builtin); maybe_docs) {
			std::cout << *maybe_docs << std::endl;
			return;
		}

		std::cerr << "musique: error: cannot find documentation for given builtin" << std::endl;
		std::exit(1);
	};

	Empty_Argument set_interactive_mode = [] { enable_repl = true; };
	Empty_Argument set_ast_only_mode = [] { ast_only_mode = true; };

	Empty_Argument print_version = [] { std::cout << Musique_Version << std::endl; };
	Empty_Argument print_help = usage;

	using Entry = std::pair<std::string_view, Parameter>;

	// First entry for given action type should always be it's cannonical name
	return std::array {
		Entry { "fun",      provide_function },
		Entry { "def",      provide_function },
		Entry { "f",        provide_function },
		Entry { "func",     provide_function },
		Entry { "function", provide_function },

		Entry { "run",  provide_file },
		Entry { "r",    provide_file },
		Entry { "exec", provide_file },

		Entry { "repl",        set_interactive_mode },
		Entry { "i",           set_interactive_mode },
		Entry { "interactive", set_interactive_mode },

		Entry { "doc",  show_docs },
		Entry { "d",    show_docs },
		Entry { "docs", show_docs },

		Entry { "inline", provide_inline_code },
		Entry { "c",      provide_inline_code },
		Entry { "code",   provide_inline_code },

		Entry { "help", print_help },
		Entry { "?",    print_help },
		Entry { "/?",   print_help },
		Entry { "h",    print_help },

		Entry { "version", print_version },
		Entry { "v",       print_version },

		Entry { "ast",     set_ast_only_mode },
	};
}();

bool cmd::accept_commandline_argument(std::vector<cmd::Run> &runnables, std::span<char const*> &args)
{
	if (args.empty())
		return false;

	for (auto const& [name, handler] : all_parameters) {
		// TODO Parameters starting with - or -- should be considered equal
		if (name != args.front()) {
			continue;
		}
		args = args.subspan(1);
		std::visit(Overloaded {
			[](Empty_Argument const& h) {
				h();
			},
			[&args, name=name](Requires_Argument const& h) {
				if (args.empty()) {
					std::cerr << "musique: error: option " << std::quoted(name) << " requires an argument" << std::endl;
					std::exit(1);
				}
				h(args.front());
				args = args.subspan(1);
			},
			[&, name=name](Defines_Code const& h) {
				if (args.empty()) {
					std::cerr << "musique: error: option " << std::quoted(name) << " requires an argument" << std::endl;
					std::exit(1);
				}
				runnables.push_back(h(args.front()));
				args = args.subspan(1);
			}
		}, handler);
		return true;
	}

	return false;
}


void cmd::print_close_matches(std::string_view arg)
{
	auto minimum_distance = std::numeric_limits<int>::max();

	std::array<typename decltype(all_parameters)::value_type, 3> closest;

	std::partial_sort_copy(
		all_parameters.begin(), all_parameters.end(),
		closest.begin(), closest.end(),
		[&minimum_distance, arg](auto const& lhs, auto const& rhs) {
			auto const lhs_score = edit_distance(arg, lhs.first);
			auto const rhs_score = edit_distance(arg, rhs.first);
			minimum_distance = std::min({ minimum_distance, lhs_score, rhs_score });
			return lhs_score < rhs_score;
		}
	);

	std::cout << "The most similar commands are:\n";
	std::unordered_set<void*> shown;
	if (minimum_distance <= 3) {
		for (auto const& [ name, handler ] : closest) {
			auto const handler_p = std::visit([](auto *v) { return reinterpret_cast<void*>(v); }, handler);
			if (!shown.contains(handler_p)) {
				std::cout << "  " << name << std::endl;
				shown.insert(handler_p);
			}
		}
	}
}

bool cmd::is_tty()
{
#ifdef _WIN32
	return _isatty(STDOUT_FILENO);
#else
	return isatty(fileno(stdout));
#endif
}

