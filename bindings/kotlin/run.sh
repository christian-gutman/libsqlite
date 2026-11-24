#!/bin/sh
# Compiles and runs the canonical Kotlin example (mirror of bindings/java).
# Usage: ./run.sh [path-to-libsqlite3]
#
# The JNA jar is the only dependency (downloaded when missing, exactly like
# the Java binding). The Kotlin compiler itself is expected on PATH or in
# KOTLINC, and the container installs it.
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
JAR="lib/jna-5.14.0.jar"
if [ ! -f "$JAR" ]; then
    mkdir -p lib
    curl -fsSL -o "$JAR" \
        https://repo1.maven.org/maven2/net/java/dev/jna/jna/5.14.0/jna-5.14.0.jar
fi

KOTLINC="${KOTLINC:-kotlinc}"
echo "$KOTLINC (JNA $(basename "$JAR")) against $LIB"
"$KOTLINC" -cp "$JAR" libsqlite.kt ExampleFull.kt -d out

exec kotlin -cp "out:$JAR:$LIBDIR" ExampleFullKt "$LIB"