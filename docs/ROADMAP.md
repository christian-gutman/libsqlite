# Roadmap — What is still pending

Status snapshot after the 1.0.0 release (tag `v1.0.0`, CI green on
Windows Win32/x64, Linux, macOS, shared and static). Ordered by effort,
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
| 9 | **API documentation site** | web | S/L | The headers already carry Doxygen comments; `doxygen` + GitHub Pages would publish them. |
| 10 | **Package managers** (vcpkg/conan ports) | all | M | The CMake package (`find_package(libsqlite3)`) and install rules already exist; a port is mostly packaging. |
| 11 | **Language bindings** (`bindings/`) | all | S→L | **In progress** — Python, C#, Go, Rust, Node.js, Java and R verified (each with a Linux container; see `docker/bindings/*.Dockerfile` and `docker/build_all.cmd`). Remaining: Dart/Flutter (deliberately last) and the rest of the modern list in `bindings/README.md`. |

Only item 1 needs a human with repository access; 5 is the next
engineering item, 6–7 extend the "one engine everywhere" story, and 11
advances one language at a time (each stage ends with a report).

---

# Hoja de ruta — Qué queda pendiente

Foto de estado tras el release 1.0.0 (tag `v1.0.0`, CI en verde en
Windows Win32/x64, Linux, macOS, compartida y estática). Ordenado por
esfuerzo, no por prioridad.

| # | Tarea | Plataforma(s) | Esfuerzo | Estado / notas |
|---|-------|---------------|----------|----------------|
| 1 | **Página de Release en GitHub** para el tag `v1.0.0` | web | 5 min | **Pendiente** — requiere acceso de escritura al repo (el tag y la entrada del `CHANGELOG.md` están listos; solo hay que crear el release desde el tag). |
| 2 | **Ejemplos de uso** (`examples/`) | todas | S | **Hecho** — `basic.c`, `parameters.c`, `transaction.c`, `json.c`, compilados por defecto y registrados como smoke tests en CTest. |
| 3 | **Publicar artefactos en CI** | CI | S | **Hecho** — `upload-artifact` en ambos jobs (shared y static, por SO/arquitectura). |
| 4 | **Contenedor de build local** (Docker) | Linux | S | **Hecho** — `docker/Dockerfile` compila shared + static y corre toda la suite CTest sin toolchains en el host. |
| 5 | **Android (NDK) + Dart/Flutter FFI** | Android | M | **Sin empezar** — compilar por ABI (`arm64-v8a`, `armeabi-v7a`, `x86_64`) con el toolchain del NDK + `LIBSQLITE3_BUILD_TESTS=OFF`; plugin; bindings con `package:ffigen`; regla de memoria: toda cadena devuelta se libera con `sqlite_core_string_free` y el callback del reader con `Pointer.fromFunction`. |
| 6 | **iOS / tvOS** | iOS | M | XCFramework estático + podspec; misma historia de FFI que Android. |
| 7 | **WASM (Emscripten)** | navegador | M | Viable: el conector es C11 puro y todo el código Windows está guardado. Restricciones: proxear el filesystem (MEMFS/IDBFS), sin extensiones (ya omitidas), y el thread-safety solo mapea a workers wasm con `-pthread` + COOP/COEP. |
| 8 | **FLL de FoxPro** | Windows | M | Entregable separado a propósito: reconstruir la FLL contra la API renombrada `sqlite_core_*` (se quitó de este repositorio en el hito 0). |
| 9 | **Sitio de docs de la API** | web | S/L | Las cabeceras ya tienen comentarios Doxygen; `doxygen` + GitHub Pages las publicaría. |
| 10 | **Gestores de paquetes** (ports vcpkg/conan) | todas | M | El paquete CMake (`find_package(libsqlite3)`) y las reglas de install ya existen; un port es sobre todo empaquetado. |
| 11 | **Bindings de lenguajes** (`bindings/`) | todas | S→L | **En progreso** — Python, C#, Go, Rust, Node.js, Java y R verificados (cada uno con su contenedor Linux; ver `docker/bindings/*.Dockerfile` y `docker/build_all.cmd`). Quedan: Dart/Flutter (a propósito al final) y el resto de la lista moderna en `bindings/README.md`. |

Solo el punto 1 necesita una persona con acceso al repo; el 5 es el
siguiente ítem de ingeniería; 6–7 extienden la historia de "un solo
motor en todas partes"; y el 11 avanza de a un lenguaje por etapa (cada
etapa termina con un informe).