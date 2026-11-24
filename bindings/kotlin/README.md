# Kotlin (JNA)

Mirror of the Java binding: `libsqlite.kt` declares the full frozen API
(34 symbols) as a JNA `Library` interface plus a Kotlin layer
(`Sqlite.ParamSet`, `Sqlite.Connection`), and `ExampleFull.kt` is the
canonical flow. Two dependencies: the **Kotlin compiler** and the **JNA jar**
(same artifact the Java binding uses).

```bash
./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

```bash
# by hand (KOTLINC points at your kotlinc, the jar is downloaded when missing)
KOTLINC=kotlinc ./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

## Container

```bat
docker\build.cmd kotlin    :: see the note below
```

`docker/bindings/kotlin.Dockerfile` uses `eclipse-temurin:17`, installs the
Kotlin compiler (GitHub release) and the JNA jar (Maven Central), builds the
library, runs the CTest suite and then compiles and runs the example with
`kotlinc` / `kotlin` against the Linux shared object.

## What the Kotlin layer adds

- `Sqlite.load(path)` loads the library through JNA (`Native.load`) after
  forcing `jna.encoding=UTF-8`, exactly like the Java binding;
- `ParamSet` builds the `sqlite_core_param_t[]` array in **native memory**
  (`setPointer` / `setInt` / `setLong` / `setDouble` at hardcoded offsets)
  and keeps the string copies alive for the call — the same approach the
  Ruby and Fortran bindings take, instead of subclassing JNA's
  `Structure`/`Union` from Kotlin (JNA's `@FieldOrder` works, but the
  raw-memory builder makes the offsets explicit and identical across
  languages);
- `Sqlite.Connection` wraps the handle (with `AutoCloseable` +
  `begin`/`commit`/`rollback`, `command`, `nonQuery`, `scalarInt`,
  `scalarText`, `readRows` (JNA `Callback` implemented in Kotlin),
  `readerJson`, `insertJson`, `getBlob`, `insertBlob`, `updateBlob`,
  `isReadonly`, `lastInsertRowid`, `setBusyTimeout`, `errorCode`,
  `errorMessage`);
- the same allocator rule as the other bindings: `takeString` reads and
  releases the SQLite-allocated strings with `sqlite_core_string_free`, and
  `staticString` copies the library's static data.

## Notes learned while building this binding

- **JNA `Callback` from Kotlin**: `fun interface RowCallback : Callback`
  (a SAM conversion) is the way to pass a Kotlin lambda where
  `sqlite_core_row_callback_fn` is expected — no `@convention(c)`-style
  trampoline is needed.
- **`char **` out** (`execute_reader_to_json`) is a `PointerByReference`
  and the heap string is read + freed with `takeString`.
- **C function pointers of interface-bound methods**: a symbol bound with
  `Native.load` cannot be re-bound as a callback argument (there is no way
  to take its address through the interface), so
  `sqlite_core_json_writer_callback` is verified only as part of the bound
  surface — the same choice the Java, C#, Go and C++ bindings make.
- **`char *` returns are `Pointer`, not `String`**: the 34-symbol interface
  returns `Pointer?` for every `const char *`, so the Kotlin layer converts
  with `getString(0, "UTF-8")` (and releases when the pointer is the
  library's heap).

## Resumen (español)

Binding Kotlin con JNA (espejo del de Java): la interfaz `Core` declara los
34 símbolos congelados con su firma exacta y `libsqlite.kt` añade
`Sqlite.ParamSet` (que construye `sqlite_core_param_t[]` en memoria nativa y
conserva las copias de nombre/valor — igual que Ruby y Fortran, evitando
subclassificar `Structure`/`Union` de JNA) y `Sqlite.Connection` (handle +
todas las operaciones). El callback de filas es un `fun interface ...
Callback` (SAM) y las cadenas devueltas se liberan con
`sqlite_core_string_free`. Verificado en el contenedor
(`eclipse-temurin:17` + Kotlin + JNA).