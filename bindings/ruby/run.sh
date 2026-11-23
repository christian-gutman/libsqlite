#!/bin/sh
# Runs the canonical Ruby example (needs the ffi gem).
# Usage: ./run.sh [path-to-libsqlite3]
set -e
cd "$(dirname "$0")"

LIB="${1:-../../bin/x64-Linux/shared/Release/libsqlite3.so}"
if [ ! -f "$LIB" ]; then
    LIB="$(find ../../bin -name 'libsqlite3.so' -o -name 'libsqlite3.dll' -o -name 'libsqlite3.dylib' 2>/dev/null | head -1)"
fi
if [ -z "$LIB" ] || [ ! -f "$LIB" ]; then
    echo "run.sh: libsqlite3 library not found (pass the path as argument)" >&2
    exit 1
fi

if ! gem list -i ffi >/dev/null 2>&1; then
    echo "installing the ffi gem"
    gem install ffi --no-document
fi

echo "ruby (ffi) against $LIB"
ruby example_full.rb "$LIB"