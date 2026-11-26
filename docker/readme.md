# Test containers

SQLite-only: every container here builds the library from this repository
and runs the suites (and one language example) against the real artifact.

| File | Purpose |
|------|---------|
| `Dockerfile` | Plain Linux build: shared + static, full CTest suite. |
| `bindings/<lang>.Dockerfile` | Builds the library, runs the suites, then compiles and runs the `bindings/<lang>` canonical example. |
| `bindings/php-windows.Dockerfile` | **Windows variant** of the PHP container (see below). Not part of CI. |
| `bindings/phpext.Dockerfile` | Builds the **Zend extension** (`bindings/phpext`) with phpize against the freshly built library, writes `30-libsqlite.ini` into the conf.d `php -i` reports, and runs the canonical example. Not part of CI. |
| `build.cmd <lang>` | Build the image, run it, **remove the image** and print `docker system df` (keeps disk usage flat). |
| `build_all.cmd [langs...]` | Runs every binding container one after the other and prints the pass/fail matrix. No arguments = all languages. |

## Not part of CI

**No binding container is wired into `.github/workflows/ci.yml`.** They all run
from `docker/build.cmd` on a machine that has Docker. This is deliberate:

- The CI matrix (Windows Win32/x64, Linux x64, shared and static) already
  covers the library and the CTest suites, which is what CI is for.
- Binding containers need one toolchain image each (dotnet, swift, rust,
  node, kotlin, julia, nimlang, gfortran, r-base, php, ...). Running the
  whole matrix in CI would multiply runner minutes for no added coverage of
  the library itself.
- macOS runners are not available to this account at all (see
  `bindings/README.md` → *Platform coverage*), so a matrix that included
  them would only add red runs.

### The Windows containers

`php.Dockerfile` (Linux) and `php-windows.Dockerfile` (Windows) are a pair:
same flow, same canonical example, different host toolchain. The shim
(`bindings/php/src/php_shim.c`) is compiled **inside each container** for the
platform that will run it, so nothing prebuilt is ever shipped.

Windows containers need a **Windows host kernel** to run: they share it, so
they cannot run on the Linux runners this project uses in CI, and they are
launched locally. Two clarifications that matter:

- They run **unattended** — `servercore` is headless, no GUI and no
  interactive session are needed.
- The host here is **Windows 10 Pro**, which supports Hyper-V and the
  Containers feature, so it is *capable* of running them.

What blocks verifying it from this machine is the engine switch, not the
platform. Docker's daemon is currently in Linux-container mode, and moving it
to Windows containers needs elevation these shells do not have:

```bat
docker\build.cmd php-windows    :: needs Docker in Windows-container mode (ask before switching the engine)
```

```bat
& 'C:\Program Files\Docker\Docker\DockerCli.exe' -SwitchWindowsEngine
:: -> switching to windows mode: context canceled   (non-elevated)
Get-WindowsOptionalFeature -Online -FeatureName Containers
:: -> La operacion solicitada requiere elevacion
```

Switching the engine is a machine-level change: **ask before running it**,
run it from an elevated shell, confirm the Containers feature is `Enabled`,
and switch back with `-SwitchLinuxEngine` when done.

> **Status:** the Linux container is verified (22 checks, 0 failures, CTest
> 7/7). The Windows container is written and documented but **not yet
> verified on a Windows host** — it needs the engine switched, which requires
> elevation. Any Windows-specific fix lands in that Dockerfile with the
> reason recorded, the same way the other containers' notes do.

```bat
docker\build.cmd python
docker\build.cmd java      :: downloads the JNA jar, runs javac + the example
docker\build.cmd r         :: compiles the R shim (R CMD SHLIB) + runs it
docker\build.cmd cpp       :: g++ -std=c++17 (header-only binding)
docker\build.cmd swift     :: swiftc -I include + module.modulemap
docker\build.cmd ruby      :: installs the ffi gem, runs the example
docker\build.cmd julia     :: ccall/@cfunction, no dependencies
docker\build_all.cmd       :: php phpext python csharp cpp go rust nodejs java r swift ruby julia kotlin nim fortran
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
| `bindings/php.Dockerfile` | `debian:bookworm-slim` + `php-cli`/`php8.2-ffi` | none (FFI is compiled in) | 26 checks |
| `bindings/python.Dockerfile` | `python:3.12-bookworm` | none (ctypes) | 23 checks |
| `bindings/csharp.Dockerfile` | `mcr.microsoft.com/dotnet/sdk:8.0` | `dotnet publish` | 23 checks |
| `bindings/go.Dockerfile` | `golang:1.22-bookworm` | cgo | 19 checks |
| `bindings/rust.Dockerfile` | `rust:1-bookworm` | plain `rustc` (the binding carries the platform-specific `#[link]` name) | 23 checks |
| `bindings/nodejs.Dockerfile` | `node:24-bookworm` | `npm install koffi` | 22 checks |
| `bindings/java.Dockerfile` | `eclipse-temurin:17-jammy` | JNA jar from Maven Central | 24 checks |
| `bindings/r.Dockerfile` | `r-base:latest` | `R CMD SHLIB` (the shim) | 25 checks |
| `bindings/swift.Dockerfile` | `swift:6.0-jammy` | none (the binding ships its own C module) | 34 checks |
| `bindings/ruby.Dockerfile` | `ruby:3.3-bookworm` | `gem install ffi` | 29 checks |
| `bindings/julia.Dockerfile` | `julia:1.11-bookworm` | none (`ccall`/`@cfunction`) | 32 checks |
| `bindings/fortran.Dockerfile` | `ubuntu:24.04` + gfortran | none (iso_c_binding) | 33 checks |
| `bindings/kotlin.Dockerfile` | `eclipse-temurin:17` + Kotlin + JNA | kotlinc + the JNA jar | 31 checks |
| `bindings/nim.Dockerfile` | `nimlang/nim:2.0.14` | none ({.importc, dynlib}) | 34 checks |
| `bindings/phpext.Dockerfile` | `debian:bookworm-slim` + `php-cli`/`php-dev` | none (phpize builds the extension; the `.ini` enables it) | 40 checks |

Check counts are from the last container run of each language and differ
by design: every example asserts a different subset of the same 34 frozen
symbols, so a lower number is not a lower coverage (see
`bindings/CONTRACT.md` §8).

**Not containerized:** Visual FoxPro (`bindings/foxpro`) and Clipper
(`bindings/clipper`). Both are Windows-only legacy consumers of the DLL
(`DECLARE DLL` / `hb_libLoad`); they need a 16/32-bit Windows runtime that
does not exist in a Linux container, so they stay on the
"declarations + README" level and are verified on Windows instead.

**The Zend extension is in the matrix** (`bindings/phpext`). Its container is
the only place on a Linux host where a PHP extension can be compiled against
this library, and it also checks something no other container can: that the
built `.so` actually declares `libsqlite3.so` as a dependency. A link line
that resolves to nothing produces a `.so` that builds, loads, and fails at
the first call — the container asserts the DT_NEEDED before writing the
`.ini`, so that failure mode has a name.

The examples mutate files only inside the container; the host repository
is never written to.
