#!/bin/sh
# Compiles and runs the canonical C++ example.
# Usage: ./run.sh [path-to-libsqlite3]
set -e
cd "$(dirname "$0")"

CORE_INC="../../src/sqlite/core"
LIB="${1:-../../bin/x64-Linux/shared/Release/libsqlite3.so}"
if [ ! -f "$LIB" ]; then
    LIB="$(find ../../bin -name 'libsqlite3.so' -o -name 'libsqlite3.dylib' 2>/dev/null | head -1)"
fi
if [ -z "$LIB" ] || [ ! -f "$LIB" ]; then
    echo "run.sh: libsqlite3 library not found (pass the path as argument)" >&2
    exit 1
fi

LIBDIR="$(cd "$(dirname "$LIB")" && pwd)"
LIBSQLITE_NAME="$(basename "$LIB")"

CXX="${CXX:-g++}"
echo "$CXX -std=c++17 against $LIBSQLITE_NAME in $LIBDIR"
"$CXX" -std=c++17 -Wall -Wextra -I . -I "$CORE_INC" example_full.cpp \
    -L "$LIBDIR" -l:"$LIBSQLITE_NAME" -Wl,-rpath,"$LIBDIR" -o example_full

./example_full "$LIB"