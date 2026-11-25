# libsqlite3 — PhpExt (Zend extension) test container.
#
# Builds the Linux shared library, then compiles the extension with phpize
# against it, and finally runs the canonical example with the extension
# loaded. Same shape as the other binding containers, and like them it is
# NOT part of CI (see docker/readme.md).
#
# A PHP-version note: a PHP extension is compiled against one specific PHP
# API version, ZTS or not. This container uses Debian's own php-dev, so
# phpize, the compiler and the php that loads the .so are all the same
# version by construction - which is the whole reason for building it here
# instead of shipping a binary.
#
# STATUS: the extension does not compile yet (the Connection class methods
# and their arginfo are missing), so this container is expected to fail at
# `make`. It exists to give that failure a place to happen: the library and
# the phpize toolchain are known-good, so any error that appears is in
# libsqlite.c, which is exactly the signal needed.

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential cmake php-cli php-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. the extension. `phpize` comes from php-dev; configure points at the
#    library CMake just produced. --with-libsqlite3-dir is config.m4's own
#    option, not a PHP one.
RUN set -e; \
    LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
    LIBDIR=$(dirname "$LIB"); \
    echo "libsqlite3 at: $LIBDIR"; \
    cd /src/bindings/phpext; \
    phpize \
    && ./configure --with-libsqlite3-src=/src/src/sqlite/core \
                   --with-libsqlite3-dir="$LIBDIR" \
    && make

# 3. the canonical example, with the extension loaded. The trailing echo must
#    not swallow the exit code: capture it and re-raise it.
WORKDIR /src/bindings/phpext
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
EXT=$(find /src/bindings/phpext -name 'libsqlite.so' | head -1); \
echo \"libsqlite3: $LIB\"; echo \"extension: $EXT\"; \
php -d extension_dir=$(dirname \"$EXT\") -d extension=libsqlite \
    example_full.php \"$LIB\" 2>&1; rc=$?; echo PHPEXT_EXIT=$rc; exit $rc"]