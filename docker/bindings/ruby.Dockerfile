# libsqlite3 — Ruby (ffi gem) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), installs the ffi gem and then the Ruby canonical example runs
# against the produced bin/x64-Linux/shared/Release/libsqlite3.so. The image
# is removed after the run (see docker/build.cmd), keeping disk usage flat.

FROM ruby:3.3-bookworm

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

# Ruby's own FFI gem (Fiddle::Closure, the standard-library alternative, is
# broken on Ruby 3.x - see bindings/ruby/README.md)
RUN gem install ffi --no-document

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Ruby canonical example; the exit code survives the echo
WORKDIR /src/bindings/ruby
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
ruby example_full.rb $LIB; rc=$?; echo RUBY_EXIT=$rc; exit $rc"]