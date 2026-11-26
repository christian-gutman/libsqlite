# libsqlite3 — PhpExt (Zend extension) test container.
#
# Builds the Linux shared library, then compiles the extension with phpize
# against it, enables it through a real .ini file, and finally runs the
# canonical example. Same shape as the other binding containers, and like
# them it is NOT part of CI (see docker/readme.md).
#
# A PHP-version note: a PHP extension is compiled against one specific PHP
# API version, ZTS or not. This container uses Debian's own php-dev, so
# phpize, the compiler and the php that loads the .so are all the same
# version by construction - which is the whole reason for building it here
# instead of shipping a binary.
#
# The .ini matters: the example must run the way a user's php runs, with the
# extension loaded by the engine's own configuration, not by a -d flag on
# the command line. That is the difference between "the .so exists" and
# "an installed extension works".

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

# 3. enable the extension the way an installation would: write the .ini into
#    the scan directory php itself reports, then prove the engine sees it.
# 3. enable the extension the way an installation would: write the .ini into
#    the scan directory php itself reports, then prove the engine sees it.
RUN set -e; \
    EXT=$(find /src/bindings/phpext -name 'libsqlite.so' | head -1); \
    echo "extension built at: $EXT"; \
    readelf -d "$EXT" | grep NEEDED; \
    echo '--- the extension must depend on libsqlite3 ---'; \
    readelf -d "$EXT" | grep -q 'libsqlite3' \
      || { echo 'FATAL: libsqlite3 is not in DT_NEEDED'; \
           echo '--- Makefile state ---'; \
           grep -nE 'LIBSQLITE|l:libsqlite|sqlite3' /src/bindings/phpext/Makefile | head -20; \
           exit 1; }; \
    CONF=$(php -i | grep -m1 'Scan this dir for additional .ini files' \
           | sed 's/.*=> //' | tr -d ' '); \
    echo "conf.d: $CONF"; \
    echo "extension=$EXT" > "$CONF/30-libsqlite.ini"; \
    cat "$CONF/30-libsqlite.ini"; \
    php -m | grep -i libsqlite

# 4. the canonical example, run by the php that now has the .ini in place.
#    The trailing echo must not swallow the exit code: capture it and
#    re-raise it.
WORKDIR /src/bindings/phpext
CMD ["sh", "-c", "php example_full.php; rc=$?; echo PHPEXT_EXIT=$rc; exit $rc"]