Tests=          \
	context.o     \
	environment.o \
	interpreter.o \
	lex.o         \
	main.o        \
	number.o      \
	parser.o      \
	unicode.o     \
	value.o

Test_Obj=$(addprefix bin/debug/tests/,$(Tests))

test: unit-tests
	scripts/test.py test examples

unit-tests: bin/unit-tests
	./$<

bin/unit-tests: $(Test_Obj) $(Debug_Obj)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(DEBUG_FLAGS) -o $@ $^

unit-test-coverage:
	@which gcov >/dev/null || ( echo "[ERROR] gcov is required for test coverage report"; false )
	@which gcovr >/dev/null || ( echo "[ERROR] gcovr is required for test coverage report"; false )
	CXXFLAGS=--coverage $(MAKE) bin/unit-tests -B
	bin/unit-tests
	rm -rf coverage
	mkdir coverage
	gcovr -e '.*\.hpp' -e 'src/tests/.*' -e 'src/pretty.cc' --html --html-details -o coverage/index.html
	rm -rf bin/debug
	xdg-open coverage/index.html
