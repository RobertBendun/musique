#!/bin/sh

if [ "$1" != "ci" ]; then
	make test
fi

lcov --directory . --capture --output-file coverage.info
lcov -r coverage.info '/usr/*'  -o coverage.info
lcov -r coverage.info '*/lib/*' -o coverage.info

if [ "$1" != "ci" ]; then
	genhtml --demangle-cpp -o coverage coverage.info
fi

