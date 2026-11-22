# libsqlite3 — Go binding test container (cgo).
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then the Go canonical example, so the binding is verified
# against a real .so. The image is removed after the run (see
# docker/build.cmd), keeping disk usage flat.

FROM golang:1.22-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Go canonical example
ENV CGO_ENABLED=1
ENV CGO_LDFLAGS="-L/src/bin/x64-Linux/shared/Release -llibsqlite3"
ENV LD_LIBRARY_PATH=/src/bin/x64-Linux/shared/Release

WORKDIR /src/bindings/go
CMD ["sh", "-c", "LIBDIR=$(dirname $(find /src/bin -name 'libsqlite3.so' | head -1)); echo \"so at: $LIBDIR\"; ls -la $LIBDIR; CGO_LDFLAGS=\"-L$LIBDIR -Wl,-rpath,$LIBDIR\" LD_LIBRARY_PATH=$LIBDIR go run ."]