# libsqlite3 — Swift binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then compiles and runs the Swift canonical example
# (include/libsqlite.h + module.modulemap -> import libsqlite3) against the
# produced libsqlite3.so. The image is removed after the run (see
# docker/build.cmd), keeping disk usage flat.

FROM swift:6.0-jammy

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Swift canonical example; the exit code survives the echo
WORKDIR /src/bindings/swift
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; LIBDIR=$(dirname $LIB); \
swiftc -I include LibSqlite.swift ExampleFull.swift main.swift \
    -L $LIBDIR -l:libsqlite3.so -Xlinker -rpath -Xlinker $LIBDIR -o main \
&& ./main; rc=$?; echo SWIFT_EXIT=$rc; exit $rc"]