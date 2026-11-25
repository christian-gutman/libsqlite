# Java (JNA)

**English** · [Español](README.es.md)

`LibSqlite.java` declares the full frozen API (34 symbols) as a JNA
`Library` interface — including `sqlite_core_param_t` (`Param`,
`ParamValue`, `BlobOrText`) and `sqlite_core_row_callback_fn`
(`RowCallback`). `ExampleFull.java` is the canonical flow. Single
dependency: the JNA jar (no native compilation).

## Dependencies

```bat
powershell -ExecutionPolicy Bypass -File get_jna.ps1   :: downloads lib\jna-5.14.0.jar
```

The jar is **not** committed (`bindings/java/lib/` is git-ignored, like
`node_modules` in the Node.js binding). The container downloads the same
artifact from Maven Central.

## Build and run (JDK 11+)

```bat
javac -encoding UTF-8 -cp lib\jna-5.14.0.jar -d classes LibSqlite.java ExampleFull.java
java -cp classes;lib\jna-5.14.0.jar ExampleFull ..\..\bin\x64-Windows\shared\Release\libsqlite3.dll
```

```bash
./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

Omitting the path uses the usual relative locations
(`../../bin/x64-Windows/shared/Release/libsqlite3.dll`, ...).

## Container

```bat
docker\build.cmd java
```

Builds the Linux library, runs the full CTest suite, then compiles and
runs this example against `bin/x64-Linux/shared/Release/libsqlite3.so`.

## Notes learned while building this binding (all applied in the files)

- **UTF-8**: set `jna.encoding=UTF-8` before `Native.load`. JNA converts
  `String` parameters and `String`/`byte[]` struct fields with the
  platform default charset otherwise — and the container locale is
  POSIX/C (ASCII), which silently breaks every non-ASCII text.
- **Structs with extra Java fields**: JNA counts *every* instance field
  as a native field, so a `Structure` can only own data that appears in
  `getFieldOrder()`. The native strings for `name` / `blob_or_text.ptr`
  are therefore pinned in a side list (`PINS`) and released after the
  call with `releaseParams()`; keeping `Memory` objects inside the struct
  throws `getFieldOrder() does not provide enough names`.
- **`import static` from the default package is rejected by javac** ("No
  field of type ... found in ..."), so the example qualifies the constants
  (`LibSqlite.SQLITE_OK`), like every other binding does.
- **Unions**: assign the struct member, then `setType(BlobOrText.class)`
  before `write()`; after a call use `read()` on the struct and on the
  union to bring `iVal` back.
- **Parameter arrays**: only `new Param().toArray(n)` produces the
  contiguous `sqlite_core_param_t[]` the C API expects — a plain
  `new Param[] {...}` allocates each element separately and only passes a
  pointer to the first one.
- **Returned `char*`**: `Pointer` + `getString(0, "UTF-8")`; heap results
  (`execute_reader_to_json`, `string_format`, ...) are read and freed with
  `takeAndFree()` → `sqlite_core_string_free`, never by the JVM.
- **Integers are 64-bit**: `sqlite_core_last_insert_rowid` returns Java
  `long`, and `insert_blob`/`update_blob` `rowid` is `long` too
  (`sqlite3_int64`), never `int`.
- **Callbacks**: implement `RowCallback` (`Callback`); the object must
  stay reachable for the whole `execute_reader` call.
- Verified locally against the x64 build: **24 checks, 0 failures**, and
  again in the Linux container (`docker\build.cmd java`).

