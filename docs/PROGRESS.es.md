# libsqlite — avances

[English](PROGRESS.md) · **Español**

*Última actualización: 2026-11-24 (commit `428063e`).*

---

## 1. Dónde estamos

`libsqlite` es un envoltorio C de SQLite, estable y multiplataforma, con
la API pública congelada en 34 símbolos (`src/sqlite/core/sqlite.def`).
Los hitos 0–5 están cerrados: higiene, compilación multiplataforma
(Win32 / Win32-static / x64 / Linux / macOS, compartida y estática),
paridad con .NET, la API congelada, la suite de pruebas con CI y la
documentación bilingüe.

El proyecto de bindings está en la **etapa 13 del plan**: 16 lenguajes
están *verificados de extremo a extremo dentro de contenedores* (se
compila la librería, corre la suite CTest y el ejemplo canónico), otros
dos (Visual FoxPro y Clipper) tienen sólo la declaración completa de los
34 símbolos y quedan excluidos a propósito del matrix de contenedores, y
los objetivos modernos y heredados restantes están pendientes.

PHP aparece dos veces a propósito, y la diferencia es el punto:
`bindings/php` es la vía FFI (sin toolchain, pero sin puntero a función C,
así que el reader pasa por un shim en C) y `bindings/phpext` es la
extensión Zend nativa (necesita phpize, y alcanza
`sqlite_core_execute_reader` con un `callable` de PHP real y
`sqlite_core_json_writer_callback` por su propia dirección).


---

## 2. Las dos decisiones que explican los números

1. **La API pública está congelada en 34 símbolos.** No se ha agregado
   nada a la DLL/componente: cada binding declara exactamente esos 34
   símbolos y ninguno extiende la librería. El fichero `sqlite.def` es
   la única fuente de verdad.

2. **Los contadores de checks difieren porque cada ejemplo ejercita un
   subconjunto distinto de esos mismos 34 símbolos.** El ejemplo canónico
   es un flujo de 19 pasos; cada lenguaje escribe sus propias
   aserciones, así que Nim reporta 34 checks, Fortran 33, Kotlin 31,
   R 25… pero todos *declaran y usan* las mismas 34 funciones, que es
   justo lo que exige `bindings/verify_symbols.py` (y lo que detectó los
   tres símbolos ausentes de Julia antes de entregarse).

---

## 3. Bindings verificados dentro de contenedores

Cada fila significa: `docker/build.cmd <lenguaje>` compila la librería
compartida de Linux, corre la suite CTest (7/7) y luego compila y corre
el ejemplo canónico. La imagen se elimina al final, manteniendo el disco
plano.

| # | Lenguaje | Binding + ejemplo | Técnica | Checks | Imagen base |
|---|----------|-------------------|---------|--------|-------------|
| 1 | Python | `python/binding.py` + `example_full.py` | `ctypes` | 23 | `python:3.12-bookworm` |
| 2 | C# | `csharp/LibSqlite.cs` + `ExampleFull.cs` | P/Invoke | 23 | `mcr.microsoft.com/dotnet/sdk:8.0` |
| 3 | C++ | `cpp/libsqlite.hpp` + `example_full.cpp` | C ABI directo | 30 | `gcc:14-bookworm` |
| 4 | Swift | `swift/LibSqlite.swift` + `ExampleFull.swift` | C ABI directo | 34 | `swift:6.0-jammy` |
| 5 | Ruby | `ruby/libsqlite.rb` + `example_full.rb` | `ffi` (Fiddle::Closure está roto en Ruby 3.x) | 29 | `ruby:3.3-bookworm` |
| 6 | Julia | `julia/libsqlite.jl` + `example_full.jl` | `ccall` + `@cfunction`, `RTLD_GLOBAL` | 32 | `julia:1.11-bookworm` |
| 7 | Kotlin | `kotlin/libsqlite.kt` + `ExampleFull.kt` | JNA (`jna.encoding=UTF-8`) | 31 | `eclipse-temurin:17-jammy` + kotlinc |
| 8 | Nim | `nim/libsqlite.nim` + `example_full.nim` | `{.importc, dynlib.}` + `{.union.}` | 34 | `nimlang/nim:2.0.14` |
| 9 | Fortran | `fortran/libsqlite.f90` + `example_full.f90` | `iso_c_binding`, `c_loc` necesita `target` | 33 | `ubuntu:24.04` + gfortran |
| 10 | Go | `go/example_full.go` (+ `go.mod`) | cgo | 19 | `golang:1.22-bookworm` |
| 11 | Rust | `rust/libsqlite.rs` + `example_full.rs` | `extern "C"` + callback `#[no_mangle]` | 23 | `rust:1-bookworm` |
| 12 | Node.js | `nodejs/libsqlite.js` + `example_full.js` | koffi | 22 | `node:24-bookworm` |
| 13 | Java | `java/LibSqlite.java` + `ExampleFull.java` | JNA | 24 | `eclipse-temurin:17-jammy` |
| 14 | R | `r/libsqlite.R` + `example_full.R` | `.Call` vía `dyn.load` | 25 | `r-base:latest` |
| 15 | PHP | `php/libsqlite.php` + `example_full.php` | FFI + el shim en C (`php/src/php_shim.c`), que cierra el mecanismo de callback que a FFI le falta | 26 | `debian:bookworm-slim` + `php-cli`/`php8.2-ffi` |
| 16 | PHP (Zend) | `phpext/libsqlite.c` + `example_full.php` | extensión nativa (`phpize`), habilitada mediante un `.ini` real | 40 | `debian:bookworm-slim` + `php-cli`/`php-dev` |

*Los checks son de la última corrida en contenedor de cada lenguaje; Fortran
(33) y Go (19) se midieron de nuevo al sincronizar la documentación, el resto
viene de su corrida de verificación original. Las 16 PASSED.*

Los contadores difieren porque cada ejemplo afirma un subconjunto distinto de
los mismos 34 símbolos — por eso un número más bajo (Go, 19) es perfectamente
compatible con el cumplimiento 17/17 de `bindings/CONTRACT.md` §8.

## 4. Declarados pero fuera del matrix

- **Visual FoxPro (VFP 9)** — `bindings/foxpro`
- **Clipper (Harbour / xHarbour)** — `bindings/clipper`

Ambos declaran los 34 símbolos (verificado por `verify_symbols.py`) pero
**no tienen contenedor**: quedan fuera del matrix a pedido del usuario.

## 5. Pendiente

| Etapa | Objetivos | Notas |
|---|---|---|
| Modernos | **D**, Haskell, Perl, Zig, LuaJIT, Tcl, MATLAB | uno por vez |
| Modernos (al final) | Dart / Flutter | diferido por el usuario |
| Heredados | Delphi / FPC, VB6, COBOL, PowerBuilder, RPG | hay que acordar el *método de verificación* (la mayoría no corre en un contenedor Linux) |

La lista completa, con la carpeta que ocupará cada uno, está en
`bindings/README.md` → *Coverage* → *Pending*.

Mantenimiento:

- mover a la fuerza el tag `v1.0.0` a HEAD, push y push de tags (patrón
  establecido al final de cada lote).
- La página de Release de GitHub para `v1.0.0`: la API respondió 401 (no
  hay PAT); necesita al usuario en la UI o un token.

## 6. Cómo verificar

```bat
docker\build.cmd nim      :: compila -> corre -> elimina la imagen
docker\build_all.cmd       :: el matrix de 16 lenguajes + impresión PASSED/FAILED
python bindings\verify_symbols.py   :: 34 símbolos contra sqlite.def
```

### Respuestas a las preguntas recurrentes

- **¿Agregaste símbolos a la DLL original / tocaste el componente?** No.
- **¿Por qué algunos muestran 19 checks y otros veintipico o treinta y
  pico?** Subconjuntos distintos de los mismos 34 símbolos.
- **¿Por qué no cambia la configuración de PRAGMAs?** Está documentada,
  no parcheada: `docs/pragmas.es.md` lista los cinco PRAGMAs que se
  aplican al abrir, sus valores medidos y los valores por defecto de
  compilación.
