## libsqlite3 binding for Nim (importc + dynlib).
##
## Complete surface of the frozen public API: the 34 symbols exported by
## src/sqlite/core/sqlite.def. No wrapper library: `importc` with `dynlib`
## makes Nim resolve the symbols in libsqlite3.so (Windows: libsqlite3.dll
## on PATH; Linux: LD_LIBRARY_PATH or the system loader).
##
## The one place that needs care is sqlite_core_param_t: it is a struct with
## a union that holds **native pointers** (name, blob_or_text.ptr). Nim has
## the `{.union.}` pragma for exactly that, and ParamSet owns the cstring
## copies so the pointers stay valid during the call.



const
  SQLITE_OK* = 0
  SQLITE_ERROR* = 1
  SQLITE_MISUSE* = 21
  SQLITE_CANTOPEN* = 14
  BUFFER_TOO_SMALL* = -2
  BLOB_TOO_SMALL* = -3
  TYPE_NULL* = 0
  TYPE_INT* = 2
  TYPE_DOUBLE* = 4
  TYPE_TEXT* = 6
  TYPE_BLOB* = 8
  DIRECTION_INPUT* = 1

type
  sqlite3* {.bycopy.} = object
  sqlite3_stmt* {.bycopy.} = object

  BlobOrText* {.bycopy.} = object
    `ptr`*: cstring
    len*: cint
    max_len*: cint

  # union { double dVal; sqlite3_int64 iVal; struct { char *ptr; int len;
  #        int max_len; } blob_or_text; } value;
  ParamValue* {.bycopy, union.} = object
    dVal*: cdouble
    iVal*: int64
    blob_or_text*: BlobOrText

  Param* {.bycopy.} = object
    name*: cstring
    no_insensitive*: cint
    `type`*: cint
    Direction*: cint
    value*: ParamValue

  RowCallback* = proc (stmt: ptr sqlite3_stmt, columns: cint, hasrows: cint,
                       ctx: pointer): cint {.cdecl.}

const libname* =
  when defined(windows):
    "libsqlite3.dll"
  elif defined(macosx):
    "libsqlite3.dylib"
  else:
    "libsqlite3.so"

# --- the 34 exported symbols (src/sqlite/core/sqlite.def) ---------------

proc sqlite_core_version*(): cstring {.importc, dynlib: libname.}
proc sqlite_core_sqlite_version*(): cstring {.importc, dynlib: libname.}

proc sqlite_core_error_code*(db: ptr sqlite3): cint {.importc, dynlib: libname.}
proc sqlite_core_error_code_extended*(db: ptr sqlite3): cint {.importc, dynlib: libname.}
proc sqlite_core_error_message*(db: ptr sqlite3): cstring {.importc, dynlib: libname.}

proc sqlite_core_string_format*(fmt: cstring, arg: cstring): cstring {.importc, dynlib: libname.}
proc sqlite_core_string_concat*(a: cstring, b: cstring): cstring {.importc, dynlib: libname.}
proc sqlite_core_string_free*(value: cstring) {.importc, dynlib: libname.}
proc sqlite_core_string_utf8_to_ansi*(text: cstring): cstring {.importc, dynlib: libname.}
proc sqlite_core_string_ansi_to_utf8*(text: cstring): cstring {.importc, dynlib: libname.}

proc sqlite_core_open*(path: cstring, out_rc: ptr cint): ptr sqlite3 {.importc, dynlib: libname.}
proc sqlite_core_open_ex*(path: cstring, flags: cint, out_rc: ptr cint): ptr sqlite3 {.importc, dynlib: libname.}
proc sqlite_core_open_readonly*(path: cstring, out_rc: ptr cint): ptr sqlite3 {.importc, dynlib: libname.}
proc sqlite_core_open_or_create*(path: cstring, out_rc: ptr cint): ptr sqlite3 {.importc, dynlib: libname.}
proc sqlite_core_open_uri_readonly*(path: cstring, out_rc: ptr cint): ptr sqlite3 {.importc, dynlib: libname.}
proc sqlite_core_close*(db: ptr sqlite3): cint {.importc, dynlib: libname.}
proc sqlite_core_is_readonly*(db: ptr sqlite3): cint {.importc, dynlib: libname.}
proc sqlite_core_last_insert_rowid*(db: ptr sqlite3): int64 {.importc, dynlib: libname.}
proc sqlite_core_set_busy_timeout*(db: ptr sqlite3, ms: cint): cint {.importc, dynlib: libname.}

proc sqlite_core_param_clean_prefix*(name: cstring): cstring {.importc, dynlib: libname.}

proc sqlite_core_execute_command*(db: ptr sqlite3, sql: cstring, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_execute_transaction*(db: ptr sqlite3, sql: cstring, txexpected: cint, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_execute_non_query*(db: ptr sqlite3, sql: cstring, params: ptr Param, count: cint, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_execute_scalar*(db: ptr sqlite3, sql: cstring, params: ptr Param, count: cint, out_scalar: ptr Param, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_execute_reader*(db: ptr sqlite3, sql: cstring, params: ptr Param, count: cint, cb: RowCallback, ctx: pointer, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}

proc sqlite_core_begin_transaction*(db: ptr sqlite3, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_commit_transaction*(db: ptr sqlite3, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_rollback_transaction*(db: ptr sqlite3, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}

proc sqlite_core_json_writer_callback*(stmt: ptr sqlite3_stmt, columns: cint, hasrows: cint, ctx: pointer): cint {.importc, dynlib: libname.}
proc sqlite_core_execute_reader_to_json*(db: ptr sqlite3, sql: cstring, params: ptr Param, count: cint, out_json: ptr cstring, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_insert_json*(db: ptr sqlite3, table_name: cstring, json_str: cstring, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}

proc sqlite_core_get_blob*(db: ptr sqlite3, sql: cstring, dest: pointer, max_len: cint, copied: ptr cint): cint {.importc, dynlib: libname.}
proc sqlite_core_insert_blob*(db: ptr sqlite3, table: cstring, column: cstring, rowid: int64, file_path: cstring, chunk: cint, written: ptr cint, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}
proc sqlite_core_update_blob*(db: ptr sqlite3, table: cstring, column: cstring, rowid: int64, file_path: cstring, chunk: cint, written: ptr cint, err_buf: pointer, err_max: cint): cint {.importc, dynlib: libname.}

# --- helpers ------------------------------------------------------------

proc errBuffer*(size: static[int] = 512): pointer =
  ## A zeroed buffer for the library's error messages.
  result = alloc0(size)

proc errText*(buf: pointer): string =
  ## The text of an err_buf filled by the library.
  let s = cast[cstring](buf)
  if s == nil:
    return ""
  $s

proc takeString*(p: cstring): string =
  ## Reads a char* allocated by the SQLite allocator and releases it with
  ## sqlite_core_string_free (never the Nim GC).
  if p == nil:
    return ""
  result = $p
  sqlite_core_string_free(p)

proc cstr*(text: string): cstring =
  ## A copy of the string valid for the call (Nim cstrings are already
  ## NUL-terminated utf-8; the copy is kept by the caller).
  cstring(text)

proc intOut*(): tuple[buf: ptr cint] =
  ## A zeroed cint plus its address (the C API writes the return code there).
  var value: cint
  (addr value, )

type
  ParamKind* = enum
    pkNull, pkInt, pkDouble, pkText

  ParamSet* = object
    ## Named parameters. Owns the cstring copies (name and value) so the
    ## native pointers stay valid during the call.
    items: seq[Param]
    owned: seq[cstring]



proc addText*(p: var ParamSet, name, value: string) =
  p.owned.add(cstr(name))
  p.owned.add(cstr(value))
  p.items.add Param(name: p.owned[^2], `type`: TYPE_TEXT, Direction: DIRECTION_INPUT)
  p.items[^1].value.blob_or_text.ptr = p.owned[^1]
  p.items[^1].value.blob_or_text.len = -1     ## NUL-terminated UTF-8

proc addInt*(p: var ParamSet, name: string, value: int64) =
  p.owned.add(cstr(name))
  p.items.add Param(name: p.owned[^1], `type`: TYPE_INT, Direction: DIRECTION_INPUT)
  p.items[^1].value.iVal = value

proc addNull*(p: var ParamSet, name: string) =
  p.owned.add(cstr(name))
  p.items.add Param(name: p.owned[^1], `type`: TYPE_NULL, Direction: DIRECTION_INPUT)

proc count*(p: ParamSet): cint = cint(p.items.len)

proc pointer*(p: var ParamSet, out_len: var cint): ptr Param =
  ## A pointer to the contiguous sqlite_core_param_t[] (nil when empty).
  out_len = p.count
  if p.items.len == 0:
    return nil
  result = addr p.items[0]

proc libraryVersion*(): string =
  ## sqlite_core_version (static data: never released).
  $sqlite_core_version()

proc sqliteVersion*(): string =
  ## sqlite_core_sqlite_version (static data: never released).
  $sqlite_core_sqlite_version()

proc paramCleanPrefix*(name: string): string =
  ## sqlite_core_param_clean_prefix: returns a pointer into the input.
  $sqlite_core_param_clean_prefix(cstr(name))