# Binding interface contract

**English** · [Español](CONTRACT.es.md)

Every binding in this folder wraps the **frozen 34-symbol API**
(`src/sqlite/core/sqlite.def`). This document is the interface contract all
of them must satisfy, so that a canonical example produces the same
behaviour in any language and the results can be compared line by line.

It covers: naming, parameters, data types, return codes, the allocator rule,
and the required assertions per step.

## 1. Exit-code contract

Every `example_full.*` **exits with the number of failed checks** (`0` on
success). This is what makes an example usable as a smoke test: a container
`CMD` that reports the exit code fails when the example fails.

> Two bugs this contract exists to prevent: `die()` in PHP exits with
> status 0, and Java's `System.exit` on an uncaught exception is non-zero but
> silent. Always print `N checks, N failures` and exit with the failure count.

## 2. The canonical flow (19 steps)

Each step names the frozen symbol it must call and what it asserts.

| # | Frozen symbol | Step asserts |
|---|---------------|--------------|
| 1 | `sqlite_core_open_or_create` | Handle is not `NULL` and `out_rc == SQLITE_OK` |
| 2 | `sqlite_core_execute_command` | `CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)` returns `SQLITE_OK` |
| 3 | `sqlite_core_execute_non_query` | `INSERT INTO person(id, name) VALUES (:id, :name)` with id `42`, name `"Ada"` → **1** row |
| 4 | `sqlite_core_execute_reader` | `SELECT id, name FROM person ORDER BY id` → callback visits **1** row; a non-zero callback return aborts |
| 5 | `sqlite_core_execute_scalar` | `SELECT COUNT(*) FROM person` → **1** |
| 6 | `sqlite_core_execute_reader_to_json` | `SELECT id, name FROM person ORDER BY id` → 1 row, document starts with `[`, contains `{"id": 42, "name": "Ada"}` |
| 7 | `sqlite_core_insert_json` | `[{"id":2,"name":"Line1"},{"id":3,"name":"Line2"}]` into `person` → **2** rows |
| 8 | `sqlite_core_insert_blob` → `sqlite_core_get_blob` → `sqlite_core_last_insert_rowid` | Fixture with embedded `0x00` bytes, 1024 bytes; `insert_blob` returns the byte count; `get_blob` round trip matches byte for byte; `last_insert_rowid > 0` |
| 9 | `sqlite_core_commit_transaction` → `sqlite_core_begin_transaction` → `sqlite_core_rollback_transaction` | commit without a transaction fails; begin succeeds; insert inside the transaction; rollback succeeds; count is back to 3 |
| 10 | `sqlite_core_update_blob` | A file twice the fixture size → `SQLITE_CORE_BLOB_TOO_SMALL` (-3) |
| 11 | `sqlite_core_open_readonly` | Opening a missing file fails with `SQLITE_CANTOPEN` |
| 12 | `sqlite_core_execute_non_query` | `SELECT 1; SELECT 2` → `SQLITE_MISUSE` |
| 13 | `sqlite_core_close` | Returns **1** |

Steps 8–13 are the "hard" half: they are where every binding has actually
found bugs.

## 3. Naming convention

The C API is `snake_case` under the `sqlite_core_` prefix. Each binding maps
it to its own idiom, but the **mapping must be one-to-one and documented in
the binding's README**. No symbol may be silently skipped: if a language
cannot reach one, say so in the README (with the reason) instead of dropping
it.

| C symbol | Python | C# | Java | Go | Rust | Node.js | Ruby | Swift | PHP (FFI) | PHP (Zend) |
|---|---|---|---|---|---|---|---|---|---|---|
| `sqlite_core_open_or_create` | `open_or_create` | `OpenOrCreate` | `openOrCreate` | `OpenOrCreate` | `open_or_create` | `openOrCreate` | `open_or_create` | `openOrCreate` | `open_or_create` | `LibSqlite\Connection::openOrCreate` |
| `sqlite_core_execute_command` | `execute_command` | `ExecuteCommand` | `executeCommand` | `ExecuteCommand` | `execute_command` | `executeCommand` | `execute_command` | `executeCommand` | `execute_command` | `Connection::command()` |
| `sqlite_core_execute_non_query` | `execute_non_query` | `ExecuteNonQuery` | `executeNonQuery` | `ExecuteNonQuery` | `execute_non_query` | `executeNonQuery` | `execute_non_query` | `executeNonQuery` | `execute_non_query` | `Connection::nonQuery()` |
| `sqlite_core_execute_scalar` | `execute_scalar` | `ExecuteScalar` | `executeScalar` | `ExecuteScalar` | `execute_scalar` | `executeScalar` | `execute_scalar` | `executeScalar` | `execute_scalar` | `Connection::scalar()` |
| `sqlite_core_execute_reader` | `execute_reader` | `ExecuteReader` | `executeReader` | `ExecuteReader` | `execute_reader` | `executeReader` | `execute_reader` | `executeReader` | `execute_reader` (via shim) | `Connection::reader()` |
| `sqlite_core_execute_reader_to_json` | `execute_reader_to_json` | `ReaderToJson` | `readerToJson` | `ReaderToJson` | `execute_reader_to_json` | `readerToJson` | `execute_reader_to_json` | `readerToJson` | `execute_reader_to_json` | `Connection::readerToJson()` |
| `sqlite_core_insert_json` | `insert_json` | `InsertJson` | `insertJson` | `InsertJson` | `insert_json` | `insertJson` | `insert_json` | `insertJson` | `insert_json` | `Connection::insertJson()` |
| `sqlite_core_get_blob` | `get_blob` | `GetBlob` | `getBlob` | `GetBlob` | `get_blob` | `getBlob` | `get_blob` | `getBlob` | `get_blob` | `Connection::getBlob()` |
| `sqlite_core_insert_blob` | `insert_blob` | `InsertBlob` | `insertBlob` | `InsertBlob` | `insert_blob` | `insertBlob` | `insert_blob` | `insertBlob` | `insert_blob` | `Connection::insertBlob()` |
| `sqlite_core_update_blob` | `update_blob` | `UpdateBlob` | `updateBlob` | `UpdateBlob` | `update_blob` | `updateBlob` | `update_blob` | `updateBlob` | `update_blob` | `Connection::updateBlob()` |
| `sqlite_core_begin_transaction` | `begin_transaction` | `BeginTransaction` | `beginTransaction` | `BeginTransaction` | `begin_transaction` | `beginTransaction` | `begin_transaction` | `beginTransaction` | `begin_transaction` | `Connection::begin()` |
| `sqlite_core_commit_transaction` | `commit_transaction` | `CommitTransaction` | `commitTransaction` | `CommitTransaction` | `commit_transaction` | `commitTransaction` | `commit_transaction` | `commitTransaction` | `commit_transaction` | `Connection::commit()` |
| `sqlite_core_rollback_transaction` | `rollback_transaction` | `RollbackTransaction` | `rollbackTransaction` | `RollbackTransaction` | `rollback_transaction` | `rollbackTransaction` | `rollback_transaction` | `rollbackTransaction` | `rollback_transaction` | `Connection::rollback()` |
| `sqlite_core_last_insert_rowid` | `last_insert_rowid` | `LastInsertRowId` | `lastInsertRowid` | `LastInsertRowId` | `last_insert_rowid` | `lastInsertRowid` | `last_insert_rowid` | `lastInsertRowid` | `last_insert_rowid` | `Connection::lastInsertRowid()` |
| `sqlite_core_set_busy_timeout` | `set_busy_timeout` | `SetBusyTimeout` | `setBusyTimeout` | `SetBusyTimeout` | `set_busy_timeout` | `setBusyTimeout` | `set_busy_timeout` | `setBusyTimeout` | `set_busy_timeout` | `Connection::setBusyTimeout()` |
| `sqlite_core_error_code` | `error_code` | `ErrorCode` | `errorCode` | `ErrorCode` | `error_code` | `errorCode` | `error_code` | `errorCode` | `error_code` | `Connection::errorCode()` |

Convention by language family: `snake_case` for the C-derived and scripting
families (Python, Julia, Nim, Ruby, R, PHP, Fortran, C++), `PascalCase` for
the .NET/Go/Java/Swift families, `camelCase` for Node.js.

## 4. Parameter contract

Binding is **always by name**, never by ordinal. A parameter missing from
the SQL is simply not bound.

A parameter is a record with three fields:

| Field | Type | Meaning |
|-------|------|---------|
| `name` | string | The parameter name, **with or without** the `:` / `@` / `?` sigil — both must work, because `sqlite_core_param_clean_prefix` strips it |
| `type` | int | One of the five `SQLITE_CORE_TYPE_*` values below |
| `value` | mixed | The value, coerced to `type`; for TEXT it is a UTF-8 string |
| `Direction` | int | `INPUT` (1), `OUTPUT` (2), `INPUTOUTPUT` (3). Default `INPUT` |

The five type values, which every binding must expose as named constants:

| Constant | Value | C type |
|----------|-------|--------|
| `SQLITE_CORE_TYPE_NULL` | 0 | — |
| `SQLITE_CORE_TYPE_INT` | 2 | `sqlite3_int64` (64-bit end to end) |
| `SQLITE_CORE_TYPE_DOUBLE` | 4 | `double` |
| `SQLITE_CORE_TYPE_TEXT` | 6 | `char *` (UTF-8, `len == -1` means NUL-terminated) |
| `SQLITE_CORE_TYPE_BLOB` | 8 | `void *` + `len` |

`sqlite_core_param_t` is `{ const char *name; int no_insensitive;
sqlite_core_param_type_t type; sqlite_core_param_direction_t Direction;
union { double dVal; sqlite3_int64 iVal; struct { char *ptr; int len; int
max_len; } blob_or_text; } value; }` — 40 bytes on LP64, 28 on ILP32. The
union's alignment matters; a binding that gets it wrong fails on one
platform only.

**TEXT results from `execute_scalar`** require the caller to pre-allocate
`value.blob_or_text.ptr` + `max_len`; larger values are silently truncated.
Without a buffer only the type is reported.

## 5. Return-code contract

From `docs/behavior.md`, unchanged:

| Return | Meaning |
|--------|---------|
| `>= 0` | Success: affected/returned rows for value functions, `1` for flag ones |
| `0` | Boolean false (`is_readonly` on a writable database) |
| `-1` | Generic error; details in `err_buf` |
| `-2` | `SQLITE_CORE_BUFFER_TOO_SMALL` (`get_blob`) |
| `-3` | `SQLITE_CORE_BLOB_TOO_SMALL` (`update_blob`, `insert_blob` upsert) |
| negative SQLite code | Engine failure (mirrors `SqlException.Number` negated) |
| positive SQLite code | `execute_*` keep the native codes (`SQLITE_MISUSE`, `SQLITE_CANTOPEN`) |

Every function that accepts `err_buf` **clears it on entry** and fills it with
the SQLite message on failure.

## 6. The allocator rule

Any `char *` the library returns from a heap-returning function
(`sqlite_core_string_format`, `sqlite_core_string_concat`,
`sqlite_core_string_utf8_to_ansi`, `sqlite_core_string_ansi_to_utf8`,
`sqlite_core_execute_reader_to_json`) is allocated by the **SQLite
allocator** and must be released with `sqlite_core_string_free` — never the
host allocator, never a GC.

Static data (`sqlite_core_version`, `sqlite_core_sqlite_version`,
`sqlite_core_error_message`, `sqlite_core_param_clean_prefix`) is owned by
the library and must **not** be freed.

## 7. Callback contract

`sqlite_core_row_callback_fn(stmt, columns, hasrows, ctx)`: one call per row,
and **exactly one call with `hasrows == 0` for an empty result set**. Return
non-zero to abort iteration.

Languages that cannot form a C function pointer from their own callables
(PHP FFI, Java/C#/Go/C++ JNA variants) must document the gap and exercise
the reader through `sqlite_core_execute_reader_to_json`, or through a small
compiled trampoline — as `bindings/r/src/libsqlite_shim.c`,
`bindings/swift` and `bindings/php/src/php_shim.c` do.

## 8. Current compliance (measured, not assumed)

Checked by scanning every `example_full.*` for the names it calls.

> **Read this before trusting a number.** This table is a **heuristic**, and
> three successive drafts of it were wrong in the same way: the scanner only
> knew one spelling per symbol. `db.command()` is `execute_command`;
> `db.read_rows()` is `execute_reader`; `db.scalar_int()` / `scalarInt` /
> `reader_count` are `execute_scalar` / `execute_reader` under another name.
> Each of those under-counted a binding that was in fact complete. **Every
> binding must confirm its own row before the number is believed** — the scan
> finds candidates, not proof.

| Binding | Observed coverage | Known gaps |
|---------|-------------------|------------|
| C++ | 17/17 | — |
| C# | 17/17 | — |
| Fortran | 17/17 | — |
| Go | 17/17 | — |
| Java | 17/17 | — |
| Julia | 17/17 | — |
| Kotlin | 17/17 | — |
| Nim | 17/17 | — |
| PHP (FFI) | 17/17 | — (the reader reaches the C API through `php/src/php_shim.c`, which closes FFI's missing callback mechanism) |
| Python | 17/17 | — |
| R | 17/17 | — (the three wrappers were added to the R-facing surface; see `bindings/r/README.md`) |
| Ruby | 17/17 | — |
| Rust | 17/17 | — |
| Swift | 17/17 | — |
| Node.js | 16/17 | `execute_scalar` is avoided **on purpose and documented** — it reads the scalar through JSON "avoids the out-struct in this example" |
| FoxPro | 15/17 | `error_code`, `error_message` |
| PHP (Zend) | 0/17 | `bindings/phpext` is step 2 of the PHP plan. The container (`docker/bindings/phpext.Dockerfile`) builds it with phpize, and §3 above fixes every method name and signature — but the `Connection` methods are unwritten, so `make` fails inside the container. No example exists yet, hence 0/17 rather than a gap list. |
| Clipper | 0/17 | `libsqlite.prg` declares the frozen surface (34/34 per `verify_symbols.py`), but `example_full.prg` is a **stub**: it contains only the helpers (`Check`, `FnLoad`, `Fixture`, `RowCount`) and calls **no symbol at all**, so the canonical flow is not implemented. The README's claim that `example_full.prg` "implements the canonical flow" is wrong. |

**Fourteen bindings reach 17/17**, each verified by running its canonical
example against the built library — in a Linux container for the modern ones
(see `docker/readme.md`), and by declaration only for the legacy ones.

**The obligations this table creates:** every row with a gap either
(a) extends its example to close it, or (b) records in its README why the
language cannot reach that symbol. A silent gap is not acceptable, because
the next person assumes the symbol was tested.

**Batch closed:** the most common gap was `set_busy_timeout`, `error_code` and
`error_message`. C#, PHP, R, Rust and Node.js now assert all three (Node.js
reading them after the deliberate multi-statement failure it already provokes),
and C++ and Swift gained `error_code`/`error_code_extended`. R also needed
those three wrappers added to its R-facing surface — the shim had them
(`sqlite_r_connection` op 2/3/4/5) but nothing exposed them.

**What remains needs either a toolchain or code this repository does not have
yet:**

- **Node.js `execute_scalar`** — a documented 16/17: the out-struct is
  avoided and the value is read through `execute_reader_to_json` instead.
  Closing it means allocating the `sqlite_core_param_t` out-parameter from
  koffi, which the binding does not do today.
- **FoxPro's `error_code`/`error_message`** — the string handling is
  VFP-specific (and its example declares `sqlite_core_string_free` as
  `STRING` while `binding.prg` declares it `void`). Needs VFP 9.
- **Clipper's canonical flow** — a stub that needs Harbour/xHarbour to write
  and run.
- **PHP (Zend)** — code, not toolchain: the `Connection` methods listed in §3.

All four are recorded as documented limitations under 8(b), because writing
them blind risks breaking an example that currently stands.

## 9. Adding a binding (checklist)

1. `bindings/<lang>/` with the binding file and `example_full.*`.
2. `README.md` (English, canonical) + `README.es.md` if it carried Spanish
   content — the two link to each other so the pair is discoverable.
3. `example_full.*` implements all 13 steps of §2 and exits with the failure
   count (§1).
4. Expose the five `SQLITE_CORE_TYPE_*` values as named constants.
5. Follow the naming convention of §3 and document the mapping in the README.
6. Register the symbol set with `python bindings/verify_symbols.py` (34/34).
7. Add a container (`docker/bindings/<lang>.Dockerfile`) and add the
   language to `docker/build_all.cmd` — **not** to CI (`docker/readme.md`).
8. Update: `bindings/README.md` coverage table, this contract's §8 table,
   `docs/PROGRESS.md`, `docs/ROADMAP.md`, `CHANGELOG.md`.

## 10. Build artifacts stay out of the repository

The scan that produced §8 tripped over compiled output in the working tree:
`bindings/java/classes/ExampleFull.class` and `bindings/swift/ExampleFull.o`.

Verified with `git ls-files` and `git check-ignore`: **neither is tracked**,
and both are already ignored (`.gitignore:34 bindings/java/classes/` and
`.gitignore:10 *.o`). So this is not a defect — it is recorded here as the
rule for new bindings: compile output goes in `.gitignore` in the same commit
as the binding, because it is stale the moment the toolchain changes and it
breaks a clean clone-and-build.
