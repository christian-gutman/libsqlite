#!/bin/sh
# Compiles and runs the canonical Fortran example.
# Usage: ./run.sh [path-to-libsqlite3]
#
# gfortran compiles the module first (it emits libsqlite.mod). The GNU
# dialect is used on purpose: STOP requires a constant code in Fortran
# 2008, and the example exits with the number of failed checks.
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

echo "gfortran against $LIBSQLITE_NAME in $LIBDIR"
gfortran -c -I . libsqlite.f90
gfortran -c -I . example_full.f90
gfortran -o example libsqlite.o example_full.o \
    -L "$LIBDIR" -l:"$LIBSQLITE_NAME" -Wl,-rpath,"$LIBDIR"

./example