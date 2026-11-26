# Roadmap — What is still pending

**English** · [Español](ROADMAP.es.md)

Status snapshot after the 1.0.0 release (tag `v1.0.0`). CI is green on
Windows Win32/x64 and Linux x64 (shared + static); macOS is not
CI-verified for lack of runner access — see item 12. Ordered by effort,
not by priority.

| # | Item | Platform(s) | Effort | Status / notes |
|---|------|-------------|--------|----------------|
| 1 | **GitHub Release page** for tag `v1.0.0` | web | 5 min | **Pending** — needs repository write access (the tag and the `CHANGELOG.md` entry are ready; just cut the release from the tag). |
| 2 | **Usage examples** (`examples/`) | all | S | **Done** — `basic.c`, `parameters.c`, `transaction.c`, `json.c`, built by default and registered as CTest smoke tests. |
| 3 | **CI artifact publishing** | CI | S | **Done** — `upload-artifact` for both jobs (shared and static, per OS/arch). |
| 4 | **Local build container** (Docker) | Linux | S | **Done** — `docker/Dockerfile` builds shared + static and runs the whole CTest suite without host toolchains. |
| 5 | **Android (NDK) + Dart/Flutter FFI** | Android | M | **Not started** — cross-compile per ABI (`arm64-v8a`, `armeabi-v7a`, `x86_64`) with the NDK toolchain file + `LIBSQLITE3_BUILD_TESTS=OFF`; plugin scaffold; bindings via `package:ffigen`; memory rule: every returned string is released with `sqlite_core_string_free`, reader callback via `Pointer.fromFunction`. |
| 6 | **iOS / tvOS** | iOS | M | Static XCFramework + podspec; same FFI story as Android. |
| 7 | **WASM (Emscripten)** | browser | M | Feasible: the connector is plain C11 and all Windows-only code is guarded. Constraints: filesystem proxying (MEMFS/IDBFS), no extensions (already omitted), thread-safety maps to wasm workers only with `-pthread` + COOP/COEP headers. |
| 8 | **FoxPro FLL** | Windows | M | Deliberate separate deliverable: rebuild the FLL against the renamed `sqlite_core_*` API (removed from this repository in milestone 0). |
| 9 | **API documentation site** | web | — | **Dropped — there will be no GitHub Pages.** The headers already carry Doxygen comments, so `doxygen` can still generate HTML locally (`doxygen Doxyfile`), but publishing it was the point and that is out of scope. Kept here so the decision is not re-litigated: if someone later wants a public site, the Doxygen comments are already in place and only the hosting is missing. |
| 10 | **Package managers** (vcpkg/conan ports) | all | M | The CMake package (`find_package(libsqlite3)`) and install rules already exist; a port is mostly packaging. |
| 11 | **Language bindings** (`bindings/`) | all | S→L | **In progress** — 15 languages verified in Linux containers (Python, C#, C++, Swift, Ruby, Julia, Kotlin, Nim, Fortran, Go, Rust, Node.js, Java, R, PHP; see `docker/bindings/*.Dockerfile` and `docker/build_all.cmd`), plus FoxPro and Clipper at the declaration level. **Next up: PHP as a Zend extension** (`bindings/phpext`) — its container already builds it with phpize and `bindings/CONTRACT.md` §3 fixes every `Connection` method name; the methods themselves are unwritten, so `make` fails inside the container until they are. Then Dart/Flutter (deliberately last) and D, Haskell, Perl, Zig, LuaJIT, Tcl, MATLAB — the full list is in `bindings/README.md`. |
| 12 | **macOS CI coverage** | CI | S | **Blocked by GitHub Actions** — `macos-14`/`macos-15` (arm64) are rejected with *"configuration `macos-14-arm64-us-default` is not supported"*, and `macos-13` (x64, the pinned label) is accepted but queues indefinitely with no runner assigned. Windows and Linux stay verified on every push; the macOS code path and the `.dylib` output are unaffected. Restoring it needs a **self-hosted Mac runner**. Details in `bindings/README.md` → *Platform coverage*. |

Only item 1 needs a human with repository access; 5 is the next
engineering item, 6–7 extend the "one engine everywhere" story, and 11
advances one language at a time (each stage ends with a report).
