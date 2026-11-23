#!/bin/sh
# Compiles the R shim against the frozen library and leaves
# libsqlite_shim.so next to this file (bindings/r/src/).
#
# Usage: ./src/build.sh [path-to-libsqlite3-library]
# The container runs the same command (see docker/bindings/r.Dockerfile).

set -e
cd "$(dirname "$0")"

CORE_INC="../../../src/sqlite/core"
LIB="${1:-../../../bin/x64-Linux/shared/Release/libsqlite3.so}"
if [ ! -f "$LIB" ]; then
    LIB="$(find ../../../bin -name 'libsqlite3.so' -o -name 'libsqlite3.dylib' 2>/dev/null | head -1)"
fi
if [ -z "$LIB" ] || [ ! -f "$LIB" ]; then
    echo "build.sh: libsqlite3 library not found (pass the path as argument)" >&2
    exit 1
fi

LIBDIR="$(cd "$(dirname "$LIB")" && pwd)"
LIBSQLITE_NAME="$(basename "$LIB")"     # libsqlite3.so / libsqlite3.dylib

echo "shim: library $LIBSQLITE_NAME in $LIBDIR, headers in $CORE_INC"
PKG_CPPFLAGS="-I$CORE_INC" \
PKG_LIBS="-L$LIBDIR -l:${LIBSQLITE_NAME} -Wl,-rpath,$LIBDIR" \
    R CMD SHLIB -o "libsqlite_shim.so" libsqlite_shim.c

echo "shim ready: src/libsqlite_shim.so"