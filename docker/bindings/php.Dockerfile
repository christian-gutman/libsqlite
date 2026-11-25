# libsqlite3 — PHP (FFI) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), compiles the ~40-line C shim that closes PHP FFI's missing
# callback mechanism, and then runs the PHP canonical example against the
# produced bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed
# after the run (see docker/build.cmd), keeping disk usage flat.
#
# NOTE: the official "php:*" images are compiled from source WITHOUT the
# FFI extension (verified: `php -m` shows no FFI, and there is no
# /etc/php/*/mods-available tree to phpenmod it). The binding therefore
# uses Debian's packaged PHP, where php8.2-ffi exists and is loaded.
#
# NOT PART OF CI: this container, like every other binding container, runs
# from docker/build.cmd on a machine that has Docker. It is deliberately
# absent from .github/workflows/ci.yml.

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake php-cli php8.2-ffi \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. PHP shim: the C trampoline that closes the FFI callback gap, compiled
#    in the container against the .so CMake just produced. It is built for
#    the platform that will run it (Linux here) -- the Windows variant is
#    documented separately and is not part of CI.
RUN set -e; \
    LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
    LIBDIR=$(dirname "$LIB"); \
    echo "shim libdir: $LIBDIR"; \
    cc -shared -fPIC -O2 \
       -I /src/src/sqlite/core \
       /src/bindings/php/src/php_shim.c \
       -L "$LIBDIR" -lsqlite3 -Wl,-rpath,"$LIBDIR" \
       -o /src/bindings/php/libsqlite_shim.so; \
    ls -la /src/bindings/php/libsqlite_shim.so

# 3. PHP canonical example (FFI + shim, no PHP packages needed).
#    The trailing "echo" must not swallow the exit code: capture it and
#    re-raise it, so "docker run" fails when the example fails.
WORKDIR /src/bindings/php
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; ls -la ./libsqlite_shim.so; \
php example_full.php $LIB ./libsqlite_shim.so; rc=$?; echo PHP_EXIT=$rc; exit $rc"]