# Local build container for libsqlite (Linux shared + static).
#
# Build and test without installing any toolchain on the host:
#   docker build -f docker/Dockerfile -t libsqlite:local .
#   docker run --rm -v "$(pwd)/bin-local:/bin-local" \
#       libsqlite:local bash -c "cp -r bin/* /bin-local/"
#
# The image builds both configurations and runs the full CTest suite
# (regression, api, exports and the four examples) before stopping.

FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Shared library (.so) + tests.
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# Static library (.a) + tests.
RUN cmake -S src/sqlite -B build-static -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build-static --parallel \
    && ctest --test-dir build-static --output-on-failure

CMD ["ctest", "--test-dir", "build", "--output-on-failure"]