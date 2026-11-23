# libsqlite3 — Python (ctypes) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then the Python canonical example against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.

FROM python:3.12-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Python canonical example (ctypes: no compilation needed).
#    The trailing "echo" must not swallow the exit code: capture it and
#    re-raise it, so "docker run" fails when the example fails.
WORKDIR /src/bindings/python
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
python3 example_full.py $LIB; rc=$?; echo PYTHON_EXIT=$rc; exit $rc"]