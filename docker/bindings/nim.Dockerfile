# libsqlite3 — Nim binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then compiles and runs the Nim canonical example against
# the produced bin/x64-Linux/shared/Release/libsqlite3.so. Nim resolves
# the library through dynlib at run time (LD_LIBRARY_PATH). The image is
# removed after the run (see docker/build.cmd), keeping disk usage flat.

FROM nimlang/nim:2.0.14

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Nim canonical example; the exit code survives the echo
WORKDIR /src/bindings/nim
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; LIBDIR=$(dirname $LIB); \
nim c -d:release --nimcache:.nimcache --out:example_full example_full.nim \
&& LD_LIBRARY_PATH=$LIBDIR ./example_full; \
rc=$?; echo NIM_EXIT=$rc; exit $rc"]