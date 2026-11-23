# libsqlite3 — R binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), compiles the R shim with `R CMD SHLIB` and then runs the R
# canonical example against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.

FROM r-base:latest

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
        r-base-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. shim + R canonical example; the exit code survives the echo
WORKDIR /src/bindings/r
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
./src/build.sh $LIB; \
Rscript example_full.R $LIB; rc=$?; echo R_EXIT=$rc; exit $rc"]