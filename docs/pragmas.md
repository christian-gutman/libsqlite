# Connection set-up: PRAGMAs, compile-time flags and what to do at open

**English** · [Español](pragmas.es.md)

Five settings are often proposed together for a "concurrent-friendly"
SQLite connection:

```sql
PRAGMA journal_mode = WAL;      -- read/write concurrency
PRAGMA synchronous = NORMAL;    -- faster commits, safe enough in WAL
PRAGMA busy_timeout = 5000;     -- wait before returning SQLITE_BUSY
PRAGMA foreign_keys = ON;       -- referential integrity
PRAGMA temp_store = MEMORY;     -- temp tables/indexes in RAM
```

This document explains **what each one does**, **where it lives** (per
connection, in the database file, or in the build), and **how libsqlite
applies them** — because the answer differs per setting.

## What each one does

| Setting | Effect | Scope |
|---------|--------|-------|
| `journal_mode = WAL` | Switches the rollback journal to a write-ahead log: readers do not block the writer and vice versa (one `-wal` + one `-shm` file appear next to the database). Requires a real file (not `:memory:`) and write access. | **Persistent in the database file** |
| `synchronous = NORMAL` | fsync policy. `FULL` fsyncs on every commit; `NORMAL` fsyncs only at checkpoints; `OFF` never. In WAL, `NORMAL` does not lose committed data on an application crash, only the last transactions on a power loss/OS crash. In rollback modes, `NORMAL` can corrupt the database on a power loss — so it is always paired with WAL. | Per connection (not persisted) |
| `busy_timeout = 5000` | When another writer holds the lock, the call waits up to 5 s before failing with `SQLITE_BUSY` instead of failing immediately. | Per connection (not persisted) |
| `foreign_keys = ON` | Enforces declared foreign keys (inserts/deletes that would leave orphan rows fail with `SQLITE_CONSTRAINT`). **OFF by default in every SQLite build.** | Per connection (never persisted) |
| `temp_store = MEMORY` | Keeps temporary tables and temporary indexes in RAM instead of a temp file (faster, more memory). | Per connection (not persisted) |

**Together**, they are the classic configuration for multi-process
concurrency with reasonable durability: WAL lets readers and one writer
coexist, `busy_timeout` turns lock contention into a wait instead of an
error, `foreign_keys` keeps the referential rules active, `temp_store`
keeps the scratch space out of the disk, and `synchronous=NORMAL` is the
fsync level that makes WAL commits cheap while surviving application
crashes.

## Where each one can be set

| Setting | At open time (code) | Compile-time default (build flag) |
|---------|---------------------|-----------------------------------|
| `journal_mode = WAL` | **Yes** — it is a normal statement, and it stays in the database file afterwards (until changed back with `PRAGMA journal_mode = DELETE`) | **No flag**: journal mode is a property of the *file*, not of the build. It can be requested per connection at run time; there is no `-DSQLITE_DEFAULT_JOURNAL_MODE` |
| `synchronous = NORMAL` | **Yes** (`PRAGMA synchronous = NORMAL`) | Partly: `-DSQLITE_DEFAULT_SYNCHRONOUS=1` (NORMAL) / `=2` (FULL), or `-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1` for WAL databases |
| `busy_timeout` | **Yes**, and already part of the API: `sqlite_core_set_busy_timeout(db, 5000)` (same effect as the PRAGMA) | **No flag**: the timeout is always per connection |
| `foreign_keys = ON` | **Yes** (`PRAGMA foreign_keys = ON`) | **Yes:** `-DSQLITE_DEFAULT_FOREIGN_KEYS=1` makes it the default of every connection |
| `temp_store = MEMORY` | **Yes** (`PRAGMA temp_store = MEMORY`) | **Yes:** `-DSQLITE_TEMP_STORE=2` (memory by default) or `=3` (memory always) |

## How libsqlite applies them today

**All five can be applied from code, at open time, with no change to the
library** — `sqlite_core_execute_command` accepts multi-statement SQL, so
the bundle is one call right after a successful open (the read-only
connections simply fail on `journal_mode`, so they skip it):

```c
sqlite3 *db = NULL;
int rc = 0;
db = sqlite_core_open_or_create("app.db", &rc);
if (db != NULL && rc == SQLITE_OK)
{
    sqlite_core_execute_command(db,
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA foreign_keys = ON;"
        "PRAGMA temp_store = MEMORY;", NULL, 0);
    sqlite_core_set_busy_timeout(db, 5000);
}
```

Notes that matter:

1. **Measured on this library** (open a file database, apply the bundle
   with one `sqlite_core_execute_command` call, read the PRAGMAs back):

   | PRAGMA | Default (verified) | After the bundle (verified) |
   |--------|--------------------|-----------------------------|
   | `journal_mode` | `delete` | `wal` — and still `wal` after reopening the file |
   | `synchronous` | `2` (FULL) | `1` (NORMAL) |
   | `foreign_keys` | `0` (**OFF: SQLite's default on every build**) | `1` |
   | `temp_store` | `0` (file) | `2` (MEMORY) |

   `sqlite_core_set_busy_timeout(db, 5000)` returns `1` (success) and an
   insert that violates a declared foreign key fails with
   `FOREIGN KEY constraint failed` — i.e. the setting is enforced, not just
   reported. `foreign_keys` reads `0` again on a new connection, confirming
   the per-connection scope, while `journal_mode` reads `wal` again on the
   same file, confirming the persistent one.
2. `busy_timeout` is a first-class function of this API
   (`sqlite_core_set_busy_timeout`, already covered by every binding), so it
   is set through the function rather than the PRAGMA.
2. Four of the five settings are **per connection**: they must be applied
   after *every* open, not once. Only `journal_mode = WAL` persists in the
   database file — and it fails on read-only databases and on `:memory:`
   connections (which are always `memory` journal mode).
3. `foreign_keys = ON` is the only one with a meaningful compile-time
   default, because SQLite turns it **off** on every build unless asked
   otherwise. A build with `-DSQLITE_DEFAULT_FOREIGN_KEYS=1` removes the
   most common "my foreign keys do nothing" support ticket.
4. `temp_store = MEMORY` and `synchronous = NORMAL` also have compile-time
   defaults, but changing them at build time changes the behaviour of every
   consumer of the library, so the documented, per-connection PRAGMA
   bundle above is the safer default to recommend; the compile-time flags
   are for applications that own the whole build.
5. `synchronous = NORMAL` only makes sense **together with WAL**. In the
   default rollback journal mode, `NORMAL` risks database corruption after
   a power loss — the pair must be set (or neither).

## Options if the bundles should be built in

Three levels, from least to most invasive (all would be a library change,
so they are proposals, not things done):

1. **Documentation only (done above)** — every consumer applies the bundle
   after opening; no binary change.
2. **A `LIBSQLITE3_RECOMMENDED_PRAGMAS` example** in
   `src/sqlite/examples` (or a snippet in each binding README) that opens,
   applies the bundle and shows the resulting `PRAGMA journal_mode` /
   `foreign_keys` values — zero risk, visible in the CTest suite.
3. **A build option** (e.g. `-DLIBSQLITE3_ENABLE_WAL_DEFAULTS=ON` adding
   `-DSQLITE_DEFAULT_FOREIGN_KEYS=1 -DSQLITE_TEMP_STORE=2
   -DSQLITE_DEFAULT_SYNCHRONOUS=1`) — only for consumers that build the
   library themselves; the flags are visible in the CMake cache and are
   off by default, so the frozen 34-symbol API and the default behaviour do
   not change.
