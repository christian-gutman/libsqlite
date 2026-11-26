# Hoja de ruta — Qué queda pendiente

> **Español.** Traducción de [`ROADMAP.md`](ROADMAP.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva
> la versión en español.

[English](ROADMAP.md) · **Español**

Foto de estado tras el release 1.0.0 (tag `v1.0.0`). El CI está en verde
en Windows Win32/x64 y Linux x64 (compartida y estática); macOS no se
verifica en el CI por falta de acceso a runners — ver el punto 12.
Ordenado por esfuerzo, no por prioridad.

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
| 9 | **Sitio de docs de la API** | web | — | **Descartado — no habrá GitHub Pages.** Las cabeceras ya llevan comentarios Doxygen, así que `doxygen Doxyfile` puede generar el HTML localmente, pero publicarlo era el objetivo y eso queda fuera de alcance. Se mantiene aquí para que la decisión no se re-litigue: si más adelante alguien quiere un sitio público, los comentarios Doxygen ya están y solo falta el hosting. |
| 10 | **Gestores de paquetes** (ports vcpkg/conan) | todas | M | El paquete CMake (`find_package(libsqlite3)`) y las reglas de install ya existen; un port es sobre todo empaquetado. |
| 11 | **Bindings de lenguajes** (`bindings/`) | todas | S→L | **En progreso** - 15 lenguajes verificados en contenedores Linux (Python, C#, C++, Swift, Ruby, Julia, Kotlin, Nim, Fortran, Go, Rust, Node.js, Java, R y PHP; ver `docker/bindings/*.Dockerfile` y `docker/build_all.cmd`), más FoxPro y Clipper al nivel de declaración. **Lo siguiente: PHP como extensión Zend** (`bindings/phpext`) - su contenedor ya lo compila con phpize y el §3 de `bindings/CONTRACT.md` fija cada nombre de método de `Connection`; los métodos en sí están sin escribir, así que `make` falla dentro del contenedor hasta que estén. Después Dart/Flutter (a propósito al final) y D, Haskell, Perl, Zig, LuaJIT, Tcl y MATLAB - la lista completa está en `bindings/README.md`. |
| 12 | **Cobertura CI de macOS** | CI | S | **Bloqueado por GitHub Actions** — `macos-14`/`macos-15` (arm64) se rechazan con *"configuration `macos-14-arm64-us-default` is not supported"*, y `macos-13` (x64, la etiqueta fijada) se acepta pero se queda en cola indefinidamente sin runner. Windows y Linux siguen verificados en cada push; la ruta de código de macOS y la salida `.dylib` no están afectadas. Recuperarlo requiere un **runner propio de Mac**. Detalle en `bindings/README.md` → *Platform coverage*. |

Solo el punto 1 necesita una persona con acceso al repo; el 5 es el
siguiente ítem de ingeniería; 6–7 extienden la historia de "un solo
motor en todas partes"; y el 11 avanza de a un lenguaje por etapa (cada
etapa termina con un informe).
