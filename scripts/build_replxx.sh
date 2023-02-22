#!/bin/sh

set -e

prefix="lib/replxx/src/"

mkdir -p "bin/${os}/replxx"

objects=""

echo "Building libreplxx library archive"

ar rcs "bin/${os}/libreplxx.a" $(find lib/replxx/src/ -name '*.cxx' -o -name '*.cpp' | while read -r src
do
	dst="${src##"$prefix"}"
	dst="bin/${os}/replxx/${dst%%.*}.o"
	"${CXX}" -Ilib/replxx/src/ -Ilib/replxx/include/ -c -o "$dst" "$src" -std=c++20 -O3 -DREPLXX_STATIC
	echo "${dst}"
done)

