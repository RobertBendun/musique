# Musique interpreter documentation

This documentation system contains information for programmers that will like to contribute or learn about Musique interpreter (and [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop)).

## Building

To build you need to have installed:

- Modern C++ compiler compatible with GNU compund expression extension and C++20 support;
- Following libraries when targetting GNU/Linux: `libreadline`, `libasound`.

Then all you have to do is run `make` in main source directory.

## Using interpreter as a library

Interpreter usage consist of three steps:

- creating chosen MIDI support for an Interpreter
- creating Interpreter instance
- parsing and evaluating source code

Parsing and evaluating may result in an error. For this case Result type offers `error()` method to get produced error and `has_value()` to check if computation was successfull.

### Simple example

We define `run` function that will do steps above, to execute provided string.

```cpp
void run(std::string_view source)
{
	Interpreter interpreter;

	// We don't want midi support so we ignore it
	interpreter.midi_connection = nullptr;

	// We set filename used in error reporting to "example"
	auto ast = Parser::parse(source, "example");

	// Check if file was properly parsed. If not print error message and exit
	if (!ast.has_value()) {
		std::cerr << ast.error() << '\n';
		return;
	}

	// Evaluate program
	auto result = interpreter.eval(std::move(ast));
	if (result.has_value()) {
		std::cout << *result << std::endl;
	} else {
		std::cerr << result.error() << '\n';
	}
}
```

When in doubt see `src/main.cc` as example on library usage.
