#!/usr/bin/env bash
# Inspired by https://dustymabe.com/2012/12/17/trace-function-calls-using-gdb-revisited/

set -e -o pipefail

if ! command -v gdb >/dev/null; then
	echo "$(basename "$1"): error: GDB is required for this script to work" 1>&2
	exit 1
fi

if [ ! -d src ]; then
	echo "$(basename "$1"): error: Expected directory 'src' in working directory" 1>&2
	exit 1
fi


tempfile=$(mktemp "${TMPDIR:-/tmp/}$(basename $0).XXXX")

echo "set confirm off" > $tempfile

for src in src/*.cc; do
	echo "rbreak $src:.
	command
	silent
	backtrace 1
	continue
	end" >> $tempfile
done

echo "run $@" >> $tempfile

set -x
make debug
gdb -quiet -batch -x "$tempfile" bin/musique
set +x
rm "$tempfile"
