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

test: bin/debug/musique
	scripts/test.py test examples

