# Test containers

SQLite-only: every container here builds the library from this repository
and runs the suites (and one language example) against the real artifact.

| File | Purpose |
|------|---------|
| `Dockerfile` | Plain Linux build: shared + static, full CTest suite. |
| `bindings/<lang>.Dockerfile` | Builds the library, runs the suites, then compiles and runs the `bindings/<lang>` canonical example. |
| `build.cmd <lang>` | Build the image, run it, **remove the image** and print `docker system df` (keeps disk usage flat). |
| `build_all.cmd [langs...]` | Runs every binding container one after the other and prints the pass/fail matrix. No arguments = all languages. |

```bat
docker\build.cmd python
docker\build.cmd java      :: downloads the JNA jar, runs javac + the example
docker\build_all.cmd       :: python csharp go rust nodejs java
```

Flow of a language container:

1. `FROM` the official toolchain image (`python`, `dotnet/sdk`, `rust`,
   `node`, `golang`, `eclipse-temurin`, ...).
2. `COPY . .` (the repository is the build context; `.dockerignore` keeps
   it small).
3. `cmake` configure + build + `ctest` (tests and examples included).
4. Compile and run the binding example against the produced
   `bin/x64-Linux/shared/Release/libsqlite3.so`.
5. The wrapper removes the image afterwards, so nothing accumulates.

## Container matrix

Every language whose example runs on Linux has a container; the example
inside the container is the same canonical flow as the host one, so the
results are comparable line by line.

| Container | Base image | Language dependency | Canonical example |
|-----------|------------|---------------------|-------------------|
| `bindings/python.Dockerfile` | `python:3.12-bookworm` | none (ctypes) | 19 checks |
| `bindings/csharp.Dockerfile` | `mcr.microsoft.com/dotnet/sdk:8.0` | `dotnet publish` | 19 checks |
| `bindings/go.Dockerfile` | `golang:1.22-bookworm` | cgo | 19 checks |
| `bindings/rust.Dockerfile` | `rust:1-bookworm` | plain `rustc` (the binding carries the platform-specific `#[link]` name) | 19 checks |
| `bindings/nodejs.Dockerfile` | `node:24-bookworm` | `npm install koffi` | 18 checks |
| `bindings/java.Dockerfile` | `eclipse-temurin:17-jammy` | JNA jar from Maven Central | 20 checks |

**Not containerized:** Visual FoxPro (`bindings/foxpro`) and Clipper
(`bindings/clipper`). Both are Windows-only legacy consumers of the DLL
(`DECLARE DLL` / `hb_libLoad`); they need a 16/32-bit Windows runtime that
does not exist in a Linux container, so they stay on the
"declarations + README" level and are verified on Windows instead.

The examples mutate files only inside the container; the host repository
is never written to.