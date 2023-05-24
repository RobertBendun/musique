#!/bin/sh

CXXFLAGS=--coverage make mode=debug
make test
lcov --directory . --capture --output-file coverage.info
lcov -r coverage.info '/usr/*'  -o coverage.info
lcov -r coverage.info '*/lib/*' -o coverage.info
genhtml --demangle-cpp -o coverage coverage.info

