#!/bin/sh
# Compiles and runs the canonical example.
# Usage: ./run.sh [path-to-libsqlite3]
# Fetches the JNA jar on demand when lib/jna-*.jar is missing.

set -e
cd "$(dirname "$0")"

JNA_VERSION="${JNA_VERSION:-5.14.0}"
JAR=$(ls lib/jna-*.jar 2>/dev/null | head -1)
if [ -z "$JAR" ]; then
    mkdir -p lib
    JAR="lib/jna-$JNA_VERSION.jar"
    if [ ! -f "$JAR" ]; then
        echo "downloading jna $JNA_VERSION"
        curl -fsSL -o "$JAR" \
            "https://repo1.maven.org/maven2/net/java/dev/jna/jna/$JNA_VERSION/jna-$JNA_VERSION.jar"
    fi
fi

echo "javac with $JAR"
javac -encoding UTF-8 -cp "$JAR" -d classes LibSqlite.java ExampleFull.java

LIB="${1:-../../bin/x64-Linux/shared/Release/libsqlite3.so}"
exec java -cp "classes:$JAR" -Djna.encoding=UTF-8 ExampleFull "$LIB"