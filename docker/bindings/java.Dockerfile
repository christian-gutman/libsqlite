# libsqlite3 — Java (JNA) binding test container.
#
# Builds the Linux shared library, runs the full CTest suite (tests +
# examples), downloads the JNA jar from Maven Central and then compiles and
# runs the Java canonical example against the produced
# bin/x64-Linux/shared/Release/libsqlite3.so. The image is removed after
# the run (see docker/build.cmd), keeping disk usage flat.

FROM eclipse-temurin:17-jammy

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# 1. library + suites (release, shared)
RUN cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 2. JNA jar (same artifact the host run uses, fetched on demand)
RUN curl -fsSL -o /opt/jna.jar \
    https://repo1.maven.org/maven2/net/java/dev/jna/jna/5.14.0/jna-5.14.0.jar \
    && echo "jna $(wc -c < /opt/jna.jar) bytes"

ENV JNA_JAR=/opt/jna.jar

# 3. Java canonical example against the real .so; the exit code survives
#    the echo so docker run reports a failure correctly
WORKDIR /src/bindings/java
CMD ["sh", "-c", "LIB=$(find /src/bin -name 'libsqlite3.so' | head -1); \
echo \"so at: $LIB\"; ls -la $LIB; \
javac -encoding UTF-8 -cp $JNA_JAR -d classes LibSqlite.java ExampleFull.java \
&& java -cp classes:$JNA_JAR -Djna.encoding=UTF-8 ExampleFull $LIB; \
rc=$?; echo JAVA_EXIT=$rc; exit $rc"]