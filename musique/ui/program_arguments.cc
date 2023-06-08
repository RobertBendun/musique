#include <algorithm>
#include <array>
#include <chrono>
#include <edit_distance.hh>
#include <iomanip>
#include <iostream>
#include <limits>
#include <musique/ui/program_arguments.hh>
#include <musique/common.hh>
#include <musique/errors.hh>
#include <musique/interpreter/builtin_function_documentation.hh>
#include <musique/pretty.hh>
#include <set>
#include <unordered_set>
#include <utility>
#include <variant>

// TODO: Command line parameters full documentation in other then man pages format. Maybe HTML generation?

#ifdef _WIN32
extern "C" {
#include <io.h>
}
#else
#include <unistd.h>
#endif

using namespace ui::program_arguments;

using Empty_Argument    = void(*)();
using Requires_Argument = void(*)(std::string_view);
using Defines_Code      = Run(*)(std::string_view);
using Parameter         = std::variant<Empty_Argument, Requires_Argument, Defines_Code>;


// from musique/main.cc:
extern bool ast_only_mode;
extern bool dont_automatically_connect;
extern bool enable_repl;
extern bool tokens_only_mode;

static Defines_Code provide_function = [](std::string_view fname) -> Run {
	return { .type = Run::Deffered_File, .argument = fname };
};

static Defines_Code provide_inline_code = [](std::string_view code) -> Run {
	return { .type = Run::Argument, .argument = code };
};

static Defines_Code provide_file = [](std::string_view fname) -> Run {
	return { .type = Run::File, .argument = fname };
};

static Requires_Argument show_docs = [](std::string_view builtin) {
	if (auto maybe_docs = find_documentation_for_builtin(builtin); maybe_docs) {
		std::cout << *maybe_docs << std::endl;
		return;
	}
	std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
	std::cerr << " cannot find documentation for given builtin" << std::endl;

	std::cerr << "Similar ones are:" << std::endl;
	for (auto similar : similar_names_to_builtin(builtin)) {
		std::cerr << "  " << similar << '\n';
	}
	std::exit(1);
};

static Empty_Argument set_interactive_mode = [] { enable_repl = true; };
static Empty_Argument set_ast_only_mode = [] { ast_only_mode = true; };
static Empty_Argument set_tokens_only_mode = [] { tokens_only_mode = true; };
static Empty_Argument set_dont_automatically_connect_mode = [] { dont_automatically_connect = true; };

static Empty_Argument print_version = [] { std::cout << Musique_Version << std::endl; };
static Empty_Argument print_help = usage;

[[noreturn]]
static void print_manpage();

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
	Entry { "run",  provide_file },
	Entry { "r",    provide_file },
	Entry { "exec", provide_file },
	Entry { "load", provide_file },

	Entry { "fun",      provide_function },
	Entry { "def",      provide_function },
	Entry { "f",        provide_function },
	Entry { "func",     provide_function },
	Entry { "function", provide_function },

	Entry { "repl",        set_interactive_mode },
	Entry { "i",           set_interactive_mode },
	Entry { "interactive", set_interactive_mode },

	Entry { "doc",  show_docs },
	Entry { "d",    show_docs },
	Entry { "docs", show_docs },

	Entry { "man", print_manpage },

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

	Entry {
		.name     = "tokens",
		.handler  = set_tokens_only_mode,
		.internal = true,
	},

	Entry {
		.name = "dont-automatically-connect",
		.handler = set_dont_automatically_connect_mode,
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
		.long_documentation =
			"Loads given file, placing it inside a function. The main use for this mechanism is\n"
			"to delay execution of a file e.g. to play it using synchronization infrastructure.\n"
			"Name of function is derived from file name, replacing special characters with underscores.\n"
			"New name is reported when entering interactive mode."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(provide_file),
		.short_documentation = "execute given file",
		.long_documentation =
			"Run provided Musique source file."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(set_interactive_mode),
		.short_documentation = "enable interactive mode",
		.long_documentation =
			"Enables interactive mode. It's enabled by default when provided without arguments or\n"
			"when all arguments are files loaded as functions."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(print_help),
		.short_documentation = "print help",
		.long_documentation =
			"Prints short version of help, to provide version easy for quick lookup by the user."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(print_version),
		.short_documentation = "print version information",
		.long_documentation =
			"Prints version of Musique, following Semantic Versioning.\n"
			"It's either '<major>.<minor>.<patch>' for official releases or\n"
			"'<major>.<minor>.<patch>-dev+gc<commit hash>' for self-build releases."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(show_docs),
		.short_documentation = "print documentation for given builtin",
		.long_documentation =
			"Prints documentation for given builtin function (function predefined by language).\n"
			"Documentation is in Markdown format and can be passed to render."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(provide_inline_code),
		.short_documentation = "run code from an argument",
		.long_documentation =
			"Runs code passed as next argument. Same rules apply as for code inside a file."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(set_ast_only_mode),
		.short_documentation = "don't run code, print AST of it",
		.long_documentation =
			"Parameter made for internal usage. Instead of executing provided code,\n"
			"prints program syntax tree."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(set_tokens_only_mode),
		.short_documentation = "don't run code, print tokens in it",
		.long_documentation =
			"Parameter made for internal usage. Instead of executing provided code,\n"
			"prints list of tokens in program."
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(set_dont_automatically_connect_mode),
		.short_documentation = "don't connect to midi port automatically",
		.long_documentation =
			"Prevents automatic connection to MIDI ports. Useful only for enviroments without audio"
	},
	Documentation_For_Handler_Entry {
		.handler = reinterpret_cast<void*>(print_manpage),
		.short_documentation = "print man page source code to standard output",
		.long_documentation =
			"Prints Man page document to standard output of Musique full command line interface.\n"
			"One can view it with 'musique man > /tmp/musique.1; man /tmp/musique.1'"
	},
};

// Supported types of argument input:
// With arity = 0
//   -i -j -k ≡ --i --j --k ≡ i j k
// With arity = 1
//   -i 1 -j 2 ≡ --i 1 --j 2 ≡ i 1 j 2 ≡ --i=1 --j=2
// Arity ≥ 2 is not supported
std::optional<std::string_view> ui::program_arguments::accept_commandline_argument(std::vector<Run> &runnables, std::span<char const*> &args)
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
					std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
					std::cerr << " option " << std::quoted(p.name) << " requires an argument" << std::endl;
					std::exit(1);
				}
				h(*arg);
			},
			[&state, &runnables, p](Defines_Code const& h) {
				auto arg = state.value();
				if (!arg) {
					std::cerr << pretty::begin_error << "musique: error:" << pretty::end;
					std::cerr << " option " << std::quoted(p.name) << " requires an argument" << std::endl;
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

void ui::program_arguments::print_close_matches(std::string_view arg)
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
			std::cout << "  " << name << " - " << find_documentation_for_parameter(name).short_documentation << '\n';
		}
	}

	std::cout << "\nInvoke 'musique help' to read more about available commands\n";
}

static inline void iterate_over_documentation(
	std::ostream& out,
	std::string_view Documentation_For_Handler_Entry::* handler,
	std::string_view prefix,
	std::ostream&(*first)(std::ostream&, std::string_view name))
{
	decltype(std::optional(all_parameters.begin())) previous = std::nullopt;

	for (auto it = all_parameters.begin();; ++it) {
		if (it != all_parameters.end() && it->internal)
			continue;

		if (it == all_parameters.end() || (previous && it->handler_ptr() != (*previous)->handler_ptr())) {
			auto &e = **previous;
			switch (e.arguments()) {
			break; case 0: out << '\n';
			break; case 1: out << " ARG\n";
			break; default: unreachable();
			}

			out << prefix << find_documentation_for_handler(e.handler_ptr()).*handler << "\n\n";
		}

		if (it == all_parameters.end()) {
			break;
		}

		if (previous && (**previous).handler_ptr() == it->handler_ptr()) {
			out << ", " << it->name;
		} else {
			first(out, it->name);
		}
		previous = it;
	}
}

void ui::program_arguments::usage()
{
	std::cerr << "usage: " << pretty::begin_bold << "musique" << pretty::end << " [subcommand]...\n";
	std::cerr << "  where available subcommands are:\n";

	iterate_over_documentation(std::cerr, &Documentation_For_Handler_Entry::short_documentation, "      ",
		[](std::ostream& out, std::string_view name) -> std::ostream&
		{
			return out << "    " << pretty::begin_bold << name << pretty::end;
		});

	std::exit(2);
}

static void print_manpage()
{
	auto const ymd = std::chrono::year_month_day(
		std::chrono::floor<std::chrono::days>(
			std::chrono::system_clock::now()
		)
	);

	std::cout << ".TH MUSIQUE 1 "
		<< int(ymd.year()) << '-'
		<< std::setfill('0') << std::setw(2) << unsigned(ymd.month()) << '-'
		<< std::setfill('0') << std::setw(2) << unsigned(ymd.day())
		<< " Linux Linux\n";

	std::cout << R"troff(.SH NAME
musique \- interactive, musical programming language
.SH SYNOPSIS
.B musique
[
SUBCOMMANDS
]
.SH DESCRIPTION
Musique is an interpreted, interactive, musical domain specific programming language
that allows for algorythmic music composition, live-coding and orchestra performing.
.SH SUBCOMMANDS
All subcommands can be expressed in three styles: -i arg -j -k
.I or
--i=arg --j --k
.I or
i arg j k
)troff";

	iterate_over_documentation(std::cout, &Documentation_For_Handler_Entry::long_documentation, {},
		[](std::ostream& out, std::string_view name) -> std::ostream&
		{
			return out << ".TP\n" << name;
		});

	std::cout << R"troff(.SH ENVIROMENT
.TP
NO_COLOR
This enviroment variable overrides standard Musique color behaviour.
When it's defined, it disables colors and ensures they are not enabled.
.SH FILES
.TP
History file
History file for interactive mode is kept in XDG_DATA_HOME (or similar on other operating systems).
.SH EXAMPLES
.TP
musique \-c "play (c5 + up 12)"
Plays all semitones in 5th octave
.TP
musique run examples/ode-to-joy.mq
Play Ode to Joy written as Musique source code in examples/ode-to-joy.mq
)troff";

	std::exit(0);
}

bool ui::program_arguments::is_tty()
{
#ifdef _WIN32
	return _isatty(STDOUT_FILENO);
#else
	return isatty(fileno(stdout));
#endif
}

