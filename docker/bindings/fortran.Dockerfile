# libsqlite3 — Fortran binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples) and then compiles and runs the Fortran canonical example
# (iso_c_binding, no other dependencies) against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.
#
# Base image note: the official gcc:<ver> images cannot install gfortran
# (the Debian "gfortran" alternatives package fails its post-installation
# script there), so this container starts from Ubuntu, where gcc and
# gfortran come from the same release and install cleanly.

FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake gfortran \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Fortran canonical example; the module is compiled first (it emits
#    libsqlite.mod) and the exit code survives the echo
WORKDIR /src/bindings/fortran
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; LIBDIR=$(dirname $LIB); \
gfortran -c -I . libsqlite.f90 \
&& gfortran -c -I . example_full.f90 \
&& gfortran -o example libsqlite.o example_full.o -L $LIBDIR -l:libsqlite3.so \
    -Wl,-rpath,$LIBDIR \
&& ./example; rc=$?; echo FORTRAN_EXIT=$rc; exit $rc"]