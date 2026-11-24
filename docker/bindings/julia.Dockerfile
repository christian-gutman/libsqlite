# libsqlite3 — Julia binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then the Julia canonical example against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. Julia needs no packages: the
# binding is pure ccall + @cfunction (the container only adds the compiler
# and cmake the library itself needs). The image is removed after the run
# (see docker/build.cmd), keeping disk usage flat.

FROM julia:1.11-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Julia canonical example; the exit code survives the echo
WORKDIR /src/bindings/julia
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
julia example_full.jl $LIB; rc=$?; echo JULIA_EXIT=$rc; exit $rc"]