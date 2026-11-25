# Swift

**English** · [Español](README.es.md)

Two files make up the binding:

- `include/libsqlite.h` + `module.modulemap` — the C module Swift imports

  (`import libsqlite3`). It declares the frozen 34-symbol surface, the
  `sqlite_core_param_t` struct (with its union) and the row callback type,
  so the binding needs nothing from the repository sources.
- `LibSqlite.swift` — the Swift layer: `Connection` (a class that closes
  the database in `deinit`), `ParamSet` (named parameters that own their
  native text) and the allocator rule in one place (`takeString`).

Verified in the Linux container (`docker\build.cmd swift`): **34 checks, 0 failures**.

`ExampleFull.swift` is the canonical flow.

## Build and run

```bash
./run.sh ../../bin/x64-Linux/shared/Release/libsqlite3.so
```

```bash
swiftc -I include LibSqlite.swift ExampleFull.swift \
    -L <libdir> -l:libsqlite3.so -Xlinker -rpath -Xlinker <libdir>
./main
```

## Container

```bat
docker\build.cmd swift
```

`swift:6.0` builds the library and the CTest suite first, then compiles and
runs the example against the Linux shared object.

## What the Swift layer adds

- `Connection` reference semantics mirror the C handle: the destructor
  closes the database, the five `sqlite_core_open*` functions map to five
  initialisers (each with an overload that reports `rc`), and the result of
  a failed open is `nil`;
- `ParamSet` copies the name and the text value into native memory
  (`strdup`) and hands out a **contiguous** `sqlite_core_param_t[]` for the
  duration of the call, releasing its copies afterwards;
- a Swift closure becomes the `sqlite_core_row_callback_fn` through a
  `@convention(c)` thunk — the captured state (the example's row counter)
  must outlive the call;
- `ScalarValue` reports which union member `execute_scalar` produced, and
  `scalarText(_:params:)` pre-allocates the destination buffer the C API
  needs for TEXT results;
- `takeString` reads and releases (with `sqlite_core_string_free`) every
  heap string the library returns; `staticString` copies the static data.

## Notes learned while building this binding

Every one of these was hit while building (and is encoded in the files):

- **The module map must live inside the include directory**
  (`include/module.modulemap`): with the map at the repository root, Swift
  reports `no such module 'libsqlite3'` even with `-I include`.
- **Incomplete C structs arrive as `OpaquePointer`**: `sqlite3*` /
  `sqlite3_stmt*` parameters are `OpaquePointer?` in Swift, so
  `RowCallback` is typed with `OpaquePointer?` for the statement. The
  `sqlite_core_param_t` struct (fully defined in the header) *is* imported.
- **A C function pointer cannot be formed from a closure that captures
  context** (Linux Swift rejects it outright), so `execute_reader` is driven
  by the module's C trampoline `libsqlite3_row_counter` + the
  `libsqlite3_row_stats` context — the same pattern as the R binding.
  `readRows(_:params:callback:context:)` stays available for callers with
  their own C callback. The trampoline's first parameter must be
  `sqlite3_stmt*` (not `void*`) or the imported signatures do not match.
- **C variadics are marked unavailable** by Swift, so
  `sqlite_core_string_format` is called through the inline wrapper
  `libsqlite3_string_format_one` declared in the module header.
- **`param.name` imports as non-optional `UnsafePointer<CChar>`**, while
  `strdup` returns an optional `UnsafeMutablePointer<CChar>`: convert with
  `UnsafePointer(nameCopy)` after a `guard let`.
- **Union member pointers**: a struct field cannot be assigned `&array`
  (only `inout` parameters accept `&`), so `scalarText` sets
  `value.blob_or_text.ptr` inside `withUnsafeMutableBufferPointer`.
- **A Swift array's pointer is only valid inside
  `withUnsafeMutableBufferPointer`**, so `ParamSet` copies its parameters
  into an allocated, stable buffer (`paramsPointer()`) instead of handing
  the array out.
- **Executable statements are only allowed in `main.swift`**: the flow lives
  in `exampleMain()` (which `throws`) and `main.swift` calls it and maps the
  return value to the exit code.
- **Empty result in `execute_scalar` is a success** with
  `SQLITE_CORE_TYPE_NULL`, so `ScalarValue.null` is a normal outcome, and
  flag-style returns are `1`, not `SQLITE_OK` (`docs/behavior.md`).
- **The library is linked, not dlopen'ed**: the linker resolves it at load
  time, which is why `run.sh` passes `-Wl,-rpath` instead of a path
  argument.

