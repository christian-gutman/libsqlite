#!/bin/sh
# Compiles and runs the canonical Nim example.
# Usage: ./run.sh [path-to-libsqlite3]
#
# The library is resolved by dynlib at run time: set LD_LIBRARY_PATH (Linux)
# or put libsqlite3.dll on PATH (Windows).
set -e
cd "$(dirname "$0")"

CORE_INC="../../src/sqlite/core"
LIB="${1:-../../bin/x64-Linux/shared/Release/libsqlite3.so}"
if [ ! -f "$LIB" ]; then
    LIB="$(find ../../bin -name 'libsqlite3.so' -o -name 'libsqlite3.dll' -o -name 'libsqlite3.dylib' 2>/dev/null | head -1)"
fi
if [ -z "$LIB" ] || [ ! -f "$LIB" ]; then
    echo "run.sh: libsqlite3 library not found (pass the path as argument)" >&2
    exit 1
fi

LIBDIR="$(cd "$(dirname "$LIB")" && pwd)"

echo "nim c against $LIB"
nim c -d:release --nimcache:.nimcache --out:example_full example_full.nim

LD_LIBRARY_PATH="$LIBDIR" ./example_full