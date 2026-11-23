# libsqlite3 — C++ binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then compiles and runs the C++ canonical example
# (header-only libsqlite.hpp) against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.

FROM gcc:14-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. C++ canonical example (header-only binding; the exit code survives the
#    echo so docker run reports a failure correctly)
WORKDIR /src/bindings/cpp
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; LIBDIR=$(dirname $LIB); \
g++ -std=c++17 -Wall -Wextra -I . -I /src/src/sqlite/core example_full.cpp \
    -L $LIBDIR -l:libsqlite3.so -Wl,-rpath,$LIBDIR -o example_full \
&& ./example_full $LIB; rc=$?; echo CPP_EXIT=$rc; exit $rc"]