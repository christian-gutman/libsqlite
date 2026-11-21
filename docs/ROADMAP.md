# Roadmap — What is still pending

Status snapshot after the 1.0.0 release (commit `b7b5b66`, CI green on
Windows Win32/x64, Linux, macOS, shared and static). Ordered by effort,
not by priority.

| # | Item | Platform(s) | Effort | Notes |
|---|------|-------------|--------|-------|
| 1 | **GitHub Release page** for tag `v1.0.0` | web | 5 min | Cut the release from the tag and paste the `CHANGELOG.md` entry: activates the Release badge and the `[1.0.0]` link. |
| 2 | **Usage examples** (`examples/`) | all | S | The folder only has a README: `basic.c`, `parameters.c`, `transaction.c`, `json.c` were planned in milestone 1 and never written. |
| 3 | **CI artifact publishing** (zip per platform) | CI | S | `.github/workflows/ci.yml` verifies artifacts but does not upload them; `actions/upload-artifact` per OS/arch. |
| 4 | **Android (NDK) + Dart/Flutter FFI** | Android | M | Cross-compile per ABI (`arm64-v8a`, `armeabi-v7a`, `x86_64`) with the NDK toolchain file + `LIBSQLITE3_BUILD_TESTS=OFF`; plugin scaffold with the `.so` prebuilt or an external CMake build; bindings via `package:ffigen`; memory rule: every returned string is released with `sqlite_core_string_free`, reader callback via `Pointer.fromFunction`. **Not started — needs an NDK test run.** |
| 5 | **iOS / tvOS** | iOS | M | Static XCFramework + podspec; same FFI story as Android. |
| 6 | **WASM (Emscripten)** | browser | M | Feasible: the connector is plain C11 and all Windows-only code is guarded; SQLite has an official WASM build path. Constraints: filesystem must be proxied (MEMFS/IDBFS), no extensions (already omitted), and thread-safety maps to wasm workers only with `-pthread` + COOP/COEP headers. |
| 7 | **FoxPro FLL** | Windows | M | Deliberate separate deliverable: rebuild the FLL against the renamed `sqlite_core_*` API (it was removed from this repository in milestone 0). |
| 8 | **API documentation site** | web | S/L | The headers already carry Doxygen comments; `doxygen` + GitHub Pages would publish them. |
| 9 | **Package managers** (vcpkg/conan ports) | all | M | The CMake package (`find_package(libsqlite3)`) and install rules already exist; a port is mostly packaging. |
| 10 | **Android test run** (CI) | CI | M | If item 4 lands, the Android build still needs emulator coverage; not required for desktop release. |

Priority suggestion: 1–3 are trivial and make the release feel finished;
4 is the only one with real user demand so far; 6 is the most interesting
demo target and pairs well with 4 (same "one engine everywhere" story).

---

# Hoja de ruta — Qué queda pendiente

Foto de estado tras el release 1.0.0 (commit `b7b5b66`, CI en verde en
Windows Win32/x64, Linux, macOS, compartida y estática). Ordenado por
esfuerzo, no por prioridad.

| # | Tarea | Plataforma(s) | Esfuerzo | Notas |
|---|-------|---------------|----------|-------|
| 1 | **Página de Release en GitHub** para el tag `v1.0.0` | web | 5 min | Crear el release desde el tag y pegar la entrada del `CHANGELOG.md`: activa el badge de Release y el enlace `[1.0.0]`. |
| 2 | **Ejemplos de uso** (`examples/`) | todas | S | La carpeta solo tiene un README: `basic.c`, `parameters.c`, `transaction.c`, `json.c` se planearon en el hito 1 y nunca se escribieron. |
| 3 | **Publicar artefactos en CI** (zip por plataforma) | CI | S | `ci.yml` solo verifica artefactos, no los sube; `actions/upload-artifact` por SO/arquitectura. |
| 4 | **Android (NDK) + Dart/Flutter FFI** | Android | M | Compilar por ABI (`arm64-v8a`, `armeabi-v7a`, `x86_64`) con el toolchain del NDK + `LIBSQLITE3_BUILD_TESTS=OFF`; plugin con el `.so` precompilado o build CMake externo; bindings con `package:ffigen`; regla de memoria: toda cadena devuelta se libera con `sqlite_core_string_free` y el callback del reader con `Pointer.fromFunction`. **Sin empezar — falta una prueba real con el NDK.** |
| 5 | **iOS / tvOS** | iOS | M | XCFramework estático + podspec; misma historia de FFI que Android. |
| 6 | **WASM (Emscripten)** | navegador | M | Viable: el conector es C11 puro y todo el código específico de Windows está guardado; SQLite tiene ruta oficial de build WASM. Restricciones: el sistema de archivos hay que proxearlo (MEMFS/IDBFS), no hay extensiones (ya omitidas), y el thread-safety solo mapea a workers wasm con `-pthread` + cabeceras COOP/COEP. |
| 7 | **FLL de FoxPro** | Windows | M | Entregable separado a propósito: reconstruir la FLL contra la API renombrada `sqlite_core_*` (se quitó de este repositorio en el hito 0). |
| 8 | **Sitio de documentación de la API** | web | S/L | Las cabeceras ya tienen comentarios Doxygen; `doxygen` + GitHub Pages las publicaría. |
| 9 | **Gestores de paquetes** (ports vcpkg/conan) | todas | M | El paquete CMake (`find_package(libsqlite3)`) y las reglas de install ya existen; un port es sobre todo empaquetado. |
| 10 | **Tests de Android en CI** | CI | M | Si aterriza el punto 4, el build Android aún necesita cobertura con emulador; no es requisito para el release de escritorio. |

Prioridad sugerida: 1–3 son triviales y dejan el release pulido; 4 es la
única con demanda real de usuarios hasta ahora; 6 es el demo más
interesante y combina con 4 (la misma historia de "un solo motor en
todas partes").