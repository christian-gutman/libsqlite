# libsqlite3 — Node.js (koffi) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), installs koffi from npm and then runs the Node.js canonical
# example against the produced bin/x64-Linux/shared/Release/libsqlite3.so.
# The image is removed after the run (see docker/build.cmd), keeping disk
# usage flat.

FROM node:24-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
        python3 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. koffi (single dependency; prebuilt when the platform has one)
WORKDIR /src/bindings/nodejs
RUN npm install --no-audit --no-fund --loglevel=error koffi

# 3. Node.js canonical example; the exit code survives the echo so docker
#    run reports a failure correctly
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
node example_full.js $LIB; rc=$?; echo NODE_EXIT=$rc; exit $rc"]