#include <algorithm>
#include <array>
#include <edit_distance.hh>
#include <iomanip>
#include <iostream>
#include <limits>
#include <musique/cmd.hh>
#include <musique/common.hh>
#include <musique/errors.hh>
#include <musique/interpreter/builtin_function_documentation.hh>
#include <musique/pretty.hh>
#include <set>
#include <unordered_set>
#include <utility>
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

static Defines_Code provide_function = [](std::string_view fname) -> cmd::Run {
	return { .type = Run::Deffered_File, .argument = fname };
};

static Defines_Code provide_inline_code = [](std::string_view code) -> cmd::Run {
	return { .type = Run::Argument, .argument = code };
};

static Defines_Code provide_file = [](std::string_view fname) -> cmd::Run {
	return { .type = Run::File, .argument = fname };
};

static Requires_Argument show_docs = [](std::string_view builtin) {
	if (auto maybe_docs = find_documentation_for_builtin(builtin); maybe_docs) {
		std::cout << *maybe_docs << std::endl;
		return;
	}
	std::cerr << "musique: error: cannot find documentation for given builtin" << std::endl;
	std::exit(1);
};

static Empty_Argument set_interactive_mode = [] { enable_repl = true; };
static Empty_Argument set_ast_only_mode = [] { ast_only_mode = true; };

static Empty_Argument print_version = [] { std::cout << Musique_Version << std::endl; };
static Empty_Argument print_help = usage;

struct Entry
{
	std::string_view name;
	Parameter handler;
	bool internal = false;

	void* handler_ptr() const
	{
		return std::visit([](auto p) { return reinterpret_cast<void*>(p); }, handler);
	}

	size_t arguments() const
	{
		return std::visit([]<typename R, typename ...A>(R(*)(A...)) { return sizeof...(A); }, handler);
	}
};

// First entry for given action type should always be it's cannonical name
static auto all_parameters = std::array {
	Entry { "fun",      provide_function },
	Entry { "def",      provide_function },
	Entry { "f",        provide_function },
	Entry { "func",     provide_function },
	Entry { "function", provide_function },

	Entry { "run",  provide_file },
	Entry { "r",    provide_file },
	Entry { "exec", provide_file },
	Entry { "load", provide_file },

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

struct Documentation_For_Handler_Entry
{
	void *handler;
	std::string_view short_documentation{};
	std::string_view long_documentation{};
};

static auto documentation_for_handler = std::array {
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(provide_function),
		.short_documentation = "load file as function",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(provide_file),
		.short_documentation = "execute given file",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(set_interactive_mode),
		.short_documentation = "enable interactive mode",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(print_help),
		.short_documentation = "print help",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(print_version),
		.short_documentation = "print version information",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(show_docs),
		.short_documentation = "print documentation for given builtin",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(provide_inline_code),
		.short_documentation = "run code from an argument",
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(set_ast_only_mode),
		.short_documentation = "don't run code, print AST of it",
	},
};

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

Documentation_For_Handler_Entry find_documentation_for_handler(void *handler)
{
	auto it = std::find_if(documentation_for_handler.begin(), documentation_for_handler.end(),
		[=](Documentation_For_Handler_Entry const& e) { return e.handler == handler; });

	ensure(it != documentation_for_handler.end(), "Parameter handler doesn't have matching documentation");
	return *it;
}

Documentation_For_Handler_Entry find_documentation_for_parameter(std::string_view param)
{
	auto entry = std::find_if(all_parameters.begin(), all_parameters.end(),
			[=](auto const& e) { return e.name == param; });

	ensure(entry != all_parameters.end(), "Cannot find parameter that maches given name");
	return find_documentation_for_handler(entry->handler_ptr());
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
			auto handler_p = p.handler_ptr();
			if (std::exchange(previous, handler_p) == handler_p || p.internal) {
				continue;
			}
			std::cout << "  " << p.name << " - " << find_documentation_for_handler(handler_p).short_documentation << '\n';
		}
	} else {
		std::cout << "The most similar commands are:\n";
		for (auto const& name : shown) {
			std::cout << "  " << name << " - " << find_documentation_for_parameter(name).short_documentation;
		}
	}

	std::cout << "Invoke 'musique help' to read more about available commands\n";
}

void cmd::usage()
{
	std::cerr << "usage: " << pretty::begin_bold << "musique" << pretty::end << " [subcommand]...\n";
	std::cerr << "  where available subcommands are:\n";

	decltype(std::optional(all_parameters.begin())) previous = std::nullopt;

	for (auto it = all_parameters.begin();; ++it) {
		if (it != all_parameters.end() && it->internal)
			continue;

		if (it == all_parameters.end() || (previous && it->handler_ptr() != (*previous)->handler_ptr())) {
			auto &e = **previous;
			switch (e.arguments()) {
			break; case 0: std::cerr << '\n';
			break; case 1: std::cerr << " ARG\n";
			break; default: unreachable();
			}

			std::cerr << "      "
				<< find_documentation_for_handler(e.handler_ptr()).short_documentation
				<< "\n\n";
		}

		if (it == all_parameters.end()) {
			break;
		}

		if (previous && (**previous).handler_ptr() == it->handler_ptr()) {
			std::cerr << ", " << it->name;
		} else {
			std::cerr << "    " << pretty::begin_bold << it->name << pretty::end;
		}
		previous = it;
	}

	std::exit(2);
}

bool cmd::is_tty()
{
#ifdef _WIN32
	return _isatty(STDOUT_FILENO);
#else
	return isatty(fileno(stdout));
#endif
}

