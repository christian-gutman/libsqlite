# libsqlite3 — Rust binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then the Rust canonical example against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.

FROM rust:1-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Rust canonical example (plain rustc: no cargo project needed); the
#    exit code survives the echo so docker run reports a failure correctly.
#    -L native= is enough: libsqlite.rs carries the platform-specific
#    #[link] name (sqlite3 on Unix -> libsqlite3.so).
WORKDIR /src/bindings/rust
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; LIBDIR=$(dirname $LIB); \
rustc -L native=$LIBDIR example_full.rs -o example \
&& LD_LIBRARY_PATH=$LIBDIR ./example; rc=$?; echo RUST_EXIT=$rc; exit $rc"]