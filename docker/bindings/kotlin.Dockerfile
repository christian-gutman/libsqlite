# libsqlite3 — Kotlin (JNA) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), installs the Kotlin compiler + the JNA jar, and compiles and
# runs the Kotlin canonical example against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.

FROM eclipse-temurin:17-jammy

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake curl unzip \
    && rm -rf /var/lib/apt/lists/*

# Kotlin compiler (the JDK above is enough to run it)
RUN curl -fsSL -o /opt/kotlin.zip \
        https://github.com/JetBrains/kotlin/releases/download/v2.1.0/kotlin-compiler-2.1.0.zip \
    && unzip -q /opt/kotlin.zip -d /opt \
    && rm /opt/kotlin.zip \
    && ln -s /opt/kotlinc/bin/kotlinc /usr/local/bin/kotlinc \
    && ln -s /opt/kotlinc/bin/kotlin /usr/local/bin/kotlin

# JNA (the only dependency of the binding, from Maven Central)
RUN curl -fsSL -o /opt/jna.jar \
        https://repo1.maven.org/maven2/net/java/dev/jna/jna/5.14.0/jna-5.14.0.jar

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. Kotlin canonical example; the exit code survives the echo
WORKDIR /src/bindings/kotlin
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; LIBDIR=$(dirname $LIB); \
kotlinc -cp /opt/jna.jar libsqlite.kt ExampleFull.kt -d out \
&& kotlin -cp out:/opt/jna.jar:$LIBDIR ExampleFullKt $LIB; \
rc=$?; echo KOTLIN_EXIT=$rc; exit $rc"]