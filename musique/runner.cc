#include <filesystem>
#include <iomanip>
#include <musique/format.hh>
#include <musique/interpreter/env.hh>
#include <musique/parser/parser.hh>
#include <musique/runner.hh>
#include <musique/try.hh>
#include <musique/unicode.hh>

bool dont_automatically_connect = false;

static std::string filename_to_function_name(std::string_view filename);

Runner::Runner()
	: interpreter{}
{
	ensure(the == nullptr, "Only one instance of runner is supported");
	the = this;

	if (!dont_automatically_connect) {
		interpreter.current_context->connect(std::nullopt);
	}

	Env::global->force_define("say", +[](Interpreter &interpreter, std::vector<Value> args) -> Result<Value> {
		for (auto it = args.begin(); it != args.end(); ++it) {
			std::cout << Try(format(interpreter, *it));
			if (std::next(it) != args.end())
				std::cout << ' ';
		}
		std::cout << std::endl;
		return {};
	});
}

extern unsigned repl_line_number;

std::optional<Error> Runner::deffered_file(std::string_view source, std::string_view filename)
{
	auto ast = Try(Parser::parse(source, filename, repl_line_number));
	auto name = filename_to_function_name(filename);

	Block block;
	// TODO: Location was replaced
	// block.location = ast.location;
	block.body = std::move(ast);
	block.context = Env::global;
	std::cout << "Defined function " << name << " as file " << filename << std::endl;
	Env::global->force_define(std::move(name), Value(std::move(block)));
	return {};
}

/// Run given source
std::optional<Error> Runner::run(std::string_view source, std::string_view filename, Execution_Options flags)
{
	flags |= default_options;

	auto ast = Try(Parser::parse(source, filename, repl_line_number));

	if (holds_alternative<Execution_Options::Print_Ast_Only>(flags)) {
		dump(ast);
		return {};
	}

	std::chrono::steady_clock::time_point now;
	try {
		if (holds_alternative<Execution_Options::Time_Execution>(flags)) {
			now = std::chrono::steady_clock::now();
		}

		if (auto result = Try(interpreter.eval(std::move(ast))); holds_alternative<Execution_Options::Print_Result>(flags) && not holds_alternative<Nil>(result)) {
			std::cout << Try(format(interpreter, result)) << std::endl;
		}
	} catch (KeyboardInterrupt const&) {
		interpreter.turn_off_all_active_notes();
		interpreter.starter.stop();
		std::cout << std::endl;
	}

	if (holds_alternative<Execution_Options::Time_Execution>(flags)) {
		auto const end = std::chrono::steady_clock::now();
		std::cout << "(" << std::fixed << std::setprecision(3) << std::chrono::duration_cast<std::chrono::duration<float>>(end - now).count() << " secs)" << std::endl;
	}

	return {};
}

std::string filename_to_function_name(std::string_view filename)
{
	if (filename == "-") {
		return "stdin";
	}

	auto stem = std::filesystem::path(filename).stem().string();
	auto stem_view = std::string_view(stem);

	std::string name;

	auto is_first = unicode::First_Character::Yes;
	while (!stem_view.empty()) {
		auto [rune, next_stem] = utf8::decode(stem_view);
		if (!unicode::is_identifier(rune, is_first)) {
			if (rune != ' ' && rune != '-') {
				// TODO Nicer error
				std::cerr << "[ERROR] Failed to construct function name from filename " << stem
					        << "\n       due to presence of invalid character: " << utf8::Print{rune} << "(U+" << std::hex << rune << ")\n"
									<< std::flush;
				std::exit(1);
			}
			name += '_';
		} else {
			name += stem_view.substr(0, stem_view.size() - next_stem.size());
		}
		stem_view = next_stem;
	}
	return name;
}
