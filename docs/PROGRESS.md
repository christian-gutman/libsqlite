# libsqlite — progress / avances

*Last updated: 2026-11-24 (commit `428063e`).*
*Última actualización: 2026-11-24 (commit `428063e`).*

---

## 1. Where we are / Dónde estamos

**EN — Executive summary**

`libsqlite` is a stable, cross-platform C SQLite wrapper around the
frozen 34-symbol public API (`src/sqlite/core/sqlite.def`). Milestones 0–5
are closed: hygiene, cross-platform build (Win32 / Win32-static / x64 /
Linux / macOS, shared + static), .NET parity, the frozen API, the test
suite with CI, and bilingual documentation.

The binding project is **stage 13 of the plan**: 14 languages are
*verified end to end inside containers* (library built, CTest suite, and
the canonical example), 2 more (Visual FoxPro and Clipper) have their
full 34-symbol declarations only and are deliberately excluded from the
container matrix, and the remaining modern and legacy targets are
pending.

**ES — Resumen ejecutivo**

`libsqlite` es un envoltorio C de SQLite, estable y multiplataforma, con
la API pública congelada en 34 símbolos (`src/sqlite/core/sqlite.def`).
Los hitos 0–5 están cerrados: higiene, compilación multiplataforma
(Win32 / Win32-static / x64 / Linux / macOS, compartida y estática),
paridad con .NET, la API congelada, la suite de pruebas con CI y la
documentación bilingüe.

El proyecto de bindings está en la **etapa 13 del plan**: 14 lenguajes
están *verificados de extremo a extremo dentro de contenedores* (se
compila la librería, corre la suite CTest y el ejemplo canónico), otros
dos (Visual FoxPro y Clipper) tienen sólo la declaración completa de los
34 símbolos y quedan excluidos a propósito del matrix de contenedores, y
los objetivos modernos y heredados restantes están pendientes.

---

## 2. The two decisions that explain the numbers / Las dos decisiones que explican los números

1. **The public API is frozen at 34 symbols.** Nothing has been added to
   `libsqlite3.dll` / `.so` / `.dylib`: every binding declares exactly
   those 34 symbols and no binding extends the library. The file
   `sqlite.def` is the single source of truth.

   **La API pública está congelada en 34 símbolos.** No se ha agregado
   nada a la DLL/componente: cada binding declara exactamente esos 34
   símbolos y ninguno extiende la librería. El fichero `sqlite.def` es
   la única fuente de verdad.

2. **The check counters differ because each example exercises a
   different subset of the same 34 symbols.** The canonical example is a
   19-step flow; every language writes its own assertions over it, so
   Nim reports 34 checks, Fortran 33, Kotlin 31, R 21… but they all
   *declare and use* the same 34 functions, which is what
   `bindings/verify_symbols.py` enforces (and what caught Julia's three
   missing symbols before they shipped).

   **Los contadores de checks difieren porque cada ejemplo ejercita un
   subconjunto distinto de esos mismos 34 símbolos.** El ejemplo canónico
   es un flujo de 19 pasos; cada lenguaje escribe sus propias
   aserciones, así que Nim reporta 34 checks, Fortran 33, Kotlin 31,
   R 21… pero todos *declaran y usan* las mismas 34 funciones, que es
   justo lo que exige `bindings/verify_symbols.py` (y lo que detectó los
   tres símbolos ausentes de Julia antes de entregarse).

---

## 3. Bindings verified inside containers / Bindings verificados dentro de contenedores

Every row below means: `docker/build.cmd <lang>` builds the Linux shared
library, runs the CTest suite (7/7) and then compiles and runs the
canonical example. The image is removed afterwards, keeping disk usage
flat.

Cada fila significa: `docker/build.cmd <lang>` compila la librería
compartida de Linux, corre la suite CTest (7/7) y luego compila y corre
el ejemplo canónico. La imagen se elimina al final, manteniendo el disco
plano.

| # | Language / Lenguaje | Binding + example | Technique / Técnica | Checks | Container base / Imagen base |
|---|---------------------|-------------------|---------------------|--------|------------------------------|
| 1 | Python | `python/binding.py` + `example_full.py` | `ctypes` | 19 | `python:3.12-bookworm` |
| 2 | C# | `csharp/LibSqlite.cs` + `ExampleFull.cs` | P/Invoke | 19 | `mcr.microsoft.com/dotnet/sdk:8.0` |
| 3 | C++ | `cpp/libsqlite.hpp` + `example_full.cpp` | direct C ABI | 28 | `gcc:14-bookworm` |
| 4 | Swift | `swift/LibSqlite.swift` + `ExampleFull.swift` | direct C ABI | 32 | `swift:6.0-jammy` |
| 5 | Ruby | `ruby/libsqlite.rb` + `example_full.rb` | `ffi` (Fiddle::Closure is broken on Ruby 3.x) | 29 | `ruby:3.3-bookworm` |
| 6 | Julia | `julia/libsqlite.jl` + `example_full.jl` | `ccall` + `@cfunction`, `RTLD_GLOBAL` | 32 | `julia:1.11-bookworm` |
| 7 | Kotlin | `kotlin/libsqlite.kt` + `ExampleFull.kt` | JNA (`jna.encoding=UTF-8`) | 31 | `eclipse-temurin:17-jammy` + kotlinc |
| 8 | Nim | `nim/libsqlite.nim` + `example_full.nim` | `{.importc, dynlib.}` + `{.union.}` | 34 | `nimlang/nim:2.0.14` |
| 9 | Fortran | `fortran/libsqlite.f90` + `example_full.f90` | `iso_c_binding`, `c_loc` needs `target` | 33 | `ubuntu:24.04` + gfortran |
| 10 | Go | `go/example_full.go` (+ `go.mod`) | cgo | 19 | `golang:1.22-bookworm` |
| 11 | Rust | `rust/libsqlite.rs` + `example_full.rs` | `extern "C"` + `#[no_mangle]` callback | 19 | `rust:1-bookworm` |
| 12 | Node.js | `nodejs/libsqlite.js` + `example_full.js` | koffi | 18 | `node:24-bookworm` |
| 13 | Java | `java/LibSqlite.java` + `ExampleFull.java` | JNA | 20 | `eclipse-temurin:17-jammy` |
| 14 | R | `r/libsqlite.R` + `example_full.R` | `.Call` via `dyn.load` | 21 | `r-base:latest` |

*Verified in the last full run (`docker/build_all.cmd`): all 14 PASSED.*
*Verificado en la última corrida completa (`docker/build_all.cmd`): las 14 PASSED.*

## 4. Declared but not in the matrix / Declarados pero fuera del matrix

- **Visual FoxPro (VFP 9)** — `bindings/foxpro`
- **Clipper (Harbour / xHarbour)** — `bindings/clipper`

Both declare all 34 symbols (verified by `verify_symbols.py`) but have
**no container**: they are excluded from the matrix at the user's
request. / Ambos declaran los 34 símbolos (verificado por
`verify_symbols.py`) pero **no tienen contenedor**: quedan fuera del
matrix a pedido del usuario.

## 5. Pending / Pendiente

| Stage / Etapa | Targets | Notes / Notas |
|---|---|---|
| Modern / Modernos | **D**, Haskell, PHP, Perl | one at a time / uno por vez |
| Modern (last) / Modernos (al final) | Dart / Flutter | deferred by the user / diferido por el usuario |
| Legacy / Heredados | Delphi / FPC, VB6, COBOL, PowerBuilder, RPG | the *verification method* must be agreed (most of these cannot run in a Linux container) / hay que acordar el *método de verificación* (la mayoría no corre en un contenedor Linux) |

Housekeeping / Mantenimiento:

- force-move tag `v1.0.0` to HEAD, push and push tags (established
  pattern at the end of each batch). / mover a la fuerza el tag
  `v1.0.0` a HEAD, push y push de tags (patrón establecido al final de
  cada lote).
- GitHub Release page for `v1.0.0`: the API answered 401 (no PAT); it
  needs either the user in the UI or a token. / La página de Release de
  GitHub para `v1.0.0`: la API respondió 401 (no hay PAT); necesita al
  usuario en la UI o un token.

## 6. How to verify / Cómo verificar

```bat
docker\build.cmd nim      :: build -> run -> remove the image
docker\build_all.cmd       :: the 14-language matrix + PASSED/FAILED print
python bindings\verify_symbols.py   :: 34 symbols vs sqlite.def
```

### Answers to the recurring questions / Respuestas a las preguntas recurrentes

- **Did you add symbols to the original DLL / touch the component?** No.
  / ¿Agregaste símbolos a la DLL original / tocaste el componente? No.
- **Why do some bindings show 19 checks and others twenty- or
  thirty-something?** Different subsets of the same 34 symbols. /
  ¿Por qué algunos muestran 19 checks y otros veintipico o treinta y
  pico? Subconjuntos distintos de los mismos 34 símbolos.
- **Why does the PRAGMA set-up not change?** It is documented, not
  patched: `docs/pragmas.md` (EN + ES) lists the five PRAGMAs applied at
  open, their measured values and the compile-time defaults. / ¿Por qué
  no cambia la configuración de PRAGMAs? Está documentada, no parcheada:
  `docs/pragmas.md` (EN + ES) lista los cinco PRAGMAs que se aplican al
  abrir, sus valores medidos y los valores por defecto de compilación.