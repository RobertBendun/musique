#!/bin/sh

set -e

prefix="lib/replxx/src/"

mkdir -p "bin/${os}/replxx"

objects=""

find lib/replxx/src/ -name '*.cxx' -o -name '*.cpp' | while read -r src
do
	dst="${src##"$prefix"}"
	dst="bin/${os}/replxx/${dst%%.*}.o"
	if [ ! -f "$dst" ]; then
		"${CXX}" -Ilib/replxx/src/ -Ilib/replxx/include/ -c -o "$dst" "$src" -std=c++20 -O3 -DREPLXX_STATIC
	fi
	echo "${dst}"
done

