#!/bin/sh
# Compiles the shim (if needed) and runs the canonical example.
# Usage: ./run.sh [path-to-libsqlite3]
set -e
cd "$(dirname "$0")"

SHIM="src/libsqlite_shim.so"
if [ ! -f "$SHIM" ]; then
    ./src/build.sh "$@"
fi

Rscript example_full.R "$@"