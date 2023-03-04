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
#include <utility>

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

	struct Entry
	{
		std::string_view name;
		Parameter handler;
		bool internal = false;
	};

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

		Entry {
			.name     = "ast",
			.handler  = set_ast_only_mode,
			.internal = true,
		},
	};
}();


// Supported types of argument input:
// With arity = 0
//   -i -j -k ≡ --i --j --k ≡ i j k
// With arity = 1
//   -i 1 -j 2 ≡ --i 1 --j 2 ≡ i 1 j 2 ≡ --i=1 --j=2
// Arity ≥ 2 is not supported
std::optional<std::string_view> cmd::accept_commandline_argument(std::vector<cmd::Run> &runnables, std::span<char const*> &args)
{
	if (args.empty()) {
		return std::nullopt;
	}

	// This struct when function returns automatically ajust number of arguments used
	struct Fix_Args_Array
	{
		std::string_view name() const { return m_name; }

		std::optional<std::string_view> value() const
		{
			if (m_has_value) { m_success = m_value_consumed = true; return m_value; }
			return std::nullopt;
		}

		void mark_success() { m_success = true; }

		~Fix_Args_Array() { args = args.subspan(int(m_success) + (!m_packed * int(m_value_consumed))); }

		std::span<char const*> &args;
		std::string_view m_name = {};
		std::string_view m_value = {};
		bool m_has_value = false;
		bool m_packed = false;
		mutable bool m_success = false;
		mutable bool m_value_consumed = false;
	} state { .args = args };

	std::string_view s = args.front();
	if (s.starts_with("--")) s.remove_prefix(2);
	if (s.starts_with('-'))  s.remove_prefix(1);

	state.m_name = s;

	if (auto p = s.find('='); p != std::string_view::npos) {
		state.m_name = s.substr(0, p);
		state.m_value = s.substr(p+1);
		state.m_has_value = true;
		state.m_packed = true;
	} else if (args.size() >= 2) {
		state.m_has_value = true;
		state.m_value = args[1];
	}

	for (auto const& p : all_parameters) {
		if (p.name != state.name()) {
			continue;
		}
		std::visit(Overloaded {
			[&state](Empty_Argument const& h) {
				state.mark_success();
				h();
			},
			[&state, p](Requires_Argument const& h) {
				auto arg = state.value();
				if (!arg) {
					std::cerr << "musique: error: option " << std::quoted(p.name) << " requires an argument" << std::endl;
					std::exit(1);
				}
				h(*arg);
			},
			[&state, &runnables, p](Defines_Code const& h) {
				auto arg = state.value();
				if (!arg) {
					std::cerr << "musique: error: option " << std::quoted(p.name) << " requires an argument" << std::endl;
					std::exit(1);
				}
				runnables.push_back(h(*arg));
			}
		}, p.handler);
		return std::nullopt;
	}

	return state.name();
}


void cmd::print_close_matches(std::string_view arg)
{
	auto minimum_distance = std::numeric_limits<int>::max();

	std::array<typename decltype(all_parameters)::value_type, 3> closest;

	std::partial_sort_copy(
		all_parameters.begin(), all_parameters.end(),
		closest.begin(), closest.end(),
		[&minimum_distance, arg](auto const& lhs, auto const& rhs) {
			auto const lhs_score = edit_distance(arg, lhs.name);
			auto const rhs_score = edit_distance(arg, rhs.name);
			minimum_distance = std::min({ minimum_distance, lhs_score, rhs_score });
			return lhs_score < rhs_score;
		}
	);

	std::vector<std::string> shown;
	if (minimum_distance <= 3) {
		for (auto const& p : closest) {
			if (std::find(shown.begin(), shown.end(), std::string(p.name)) == shown.end()) {
				shown.push_back(std::string(p.name));
			}
		}
	}

	if (shown.empty()) {
		void *previous = nullptr;
		std::cout << "Available subcommands are:\n";
		for (auto const& p : all_parameters) {
			auto handler_p = std::visit([](auto const& h) { return reinterpret_cast<void*>(h); }, p.handler);
			if (std::exchange(previous, handler_p) == handler_p || p.internal) {
				continue;
			}
			std::cout << "  " << p.name << '\n';
		}

	} else {
		std::cout << "The most similar commands are:\n";
		for (auto const& name : shown) {
			std::cout << "  " << name << '\n';
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

