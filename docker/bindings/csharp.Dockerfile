# libsqlite3 — C# (.NET) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), publishes the C# example and runs it with libsqlite3.so next
# to the assembly (DllImport resolves it from the app directory). The
# image is removed after the run (see docker/build.cmd), keeping disk
# usage flat.

FROM mcr.microsoft.com/dotnet/sdk:8.0

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. publish first, so the image layer can be cached
WORKDIR /src/bindings/csharp
RUN dotnet publish -c Release -o out --nologo

# 3. the .so goes next to the assembly and the example runs; the exit code
#    survives the echo, so docker run reports a failure correctly
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
cp $LIB out/libsqlite3.so && dotnet out/csharp.dll; rc=$?; \
echo CSHARP_EXIT=$rc; exit $rc"]