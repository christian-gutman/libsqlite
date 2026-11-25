# libsqlite3 binding for R.
#
# Pure R cannot reach the frozen API: R's .C() discards the C return value
# (so the sqlite3* handle would be unreachable), cannot build
# sqlite_core_param_t (a struct whose union holds *native pointers*), has no
# representation for sqlite3_int64, and cannot turn an R closure into the
# C callback that sqlite_core_execute_reader needs.
#
# The binding therefore ships two layers:
#
#   1. src/libsqlite_shim.c - compiled once with `R CMD SHLIB`; it calls the
#      frozen API and marshals the types (see the comment at the top of that
#      file). The 34 exported symbols stay the single surface under test.
#   2. this file - the R wrappers around that shim, called through
#      getNativeSymbolInfo() (the shim dll is not a registered package).
#
# SYMBOLS below lists the frozen surface of the library: the 34 exports of
# src/sqlite/core/sqlite.def, also checked by bindings/verify_symbols.py.

SYMBOLS <- c(
  "sqlite_core_error_code",
  "sqlite_core_error_code_extended",
  "sqlite_core_error_message",
  "sqlite_core_string_format",
  "sqlite_core_string_concat",
  "sqlite_core_string_free",
  "sqlite_core_string_utf8_to_ansi",
  "sqlite_core_string_ansi_to_utf8",
  "sqlite_core_open_readonly",
  "sqlite_core_open_ex",
  "sqlite_core_open_or_create",
  "sqlite_core_open",
  "sqlite_core_open_uri_readonly",
  "sqlite_core_close",
  "sqlite_core_is_readonly",
  "sqlite_core_last_insert_rowid",
  "sqlite_core_set_busy_timeout",
  "sqlite_core_param_clean_prefix",
  "sqlite_core_execute_transaction",
  "sqlite_core_execute_non_query",
  "sqlite_core_execute_scalar",
  "sqlite_core_execute_reader",
  "sqlite_core_execute_command",
  "sqlite_core_begin_transaction",
  "sqlite_core_commit_transaction",
  "sqlite_core_rollback_transaction",
  "sqlite_core_version",
  "sqlite_core_sqlite_version",
  "sqlite_core_json_writer_callback",
  "sqlite_core_execute_reader_to_json",
  "sqlite_core_insert_json",
  "sqlite_core_get_blob",
  "sqlite_core_insert_blob",
  "sqlite_core_update_blob"
)

LIBSQLITE_REL <- c(
  "libsqlite3.so", "libsqlite3.dylib",
  "../../bin/x64-Linux/shared/Release/libsqlite3.so",
  "../../bin/x64-Windows/shared/Release/libsqlite3.dll",
  "../../bin/arm64-Darwin/shared/Release/libsqlite3.dylib"
)

# State: the DLLInfo of the compiled shim (the libsqlite3 handle is opened
# first because the shim links against it at load time).
.SHIM <- new.env(parent = emptyenv())

libsqlite_load <- function(path = NULL, shim = NULL) {
  if (is.null(path)) {
    found <- LIBSQLITE_REL[file.exists(LIBSQLITE_REL)]
    if (length(found) == 0) {
      stop("libsqlite3 library not found; pass the path explicitly")
    }
    path <- found[1]
  }
  if (!file.exists(path)) {
    stop("libsqlite3 library not found: ", path)
  }

  if (is.null(shim)) {
    candidates <- c(
      file.path("src", paste0("libsqlite_shim", .Platform$dynlib.ext)),
      file.path("..", "r", "src", paste0("libsqlite_shim", .Platform$dynlib.ext))
    )
    found <- candidates[file.exists(candidates)]
    if (length(found) == 0) {
      stop("compiled shim not found; run src/build.sh first")
    }
    shim <- found[1]
  }

  lib <- dyn.load(normalizePath(path))
  dll <- dyn.load(normalizePath(shim))
  .SHIM$lib <- lib
  .SHIM$dll <- dll
  .SHIM$path <- path
  .SHIM$shim <- shim
  invisible(list(lib = lib, path = path, shim = shim))
}

# Calls one shim entry point. The shim exposes the frozen API with R
# marshalling; the names below are only the entry points, and every one of
# them ends up calling the symbols listed in SYMBOLS.
shim_call <- function(name, ...) {
  if (is.null(.SHIM$dll)) {
    stop("call libsqlite_load() first")
  }
  # The shim is not a registered package: the DLLInfo from dyn.load() goes
  # in as PACKAGE (there is no "DLL" argument in getNativeSymbolInfo).
  sym <- getNativeSymbolInfo(name, PACKAGE = .SHIM$dll)
  .Call(sym, ...)
}

# --- parameters (R-facing spelling of sqlite_core_param_t) --------------
param <- function(name, type, value) {
  list(name = as.character(name), type = as.character(type), value = value)
}
int_param <- function(name, value) param(name, "int", value)
text_param <- function(name, value) param(name, "text", as.character(value))
null_param <- function(name) param(name, "null", NULL)

# --- connection ----------------------------------------------------------
# mode 0 open, 1 open_ex(flags), 2 readonly, 3 open_or_create, 4 uri_readonly
libsqlite_open <- function(path, readonly = FALSE, create = TRUE, flags = 0) {
  mode <- if (isTRUE(readonly)) 2L else if (isTRUE(create)) 3L else 0L
  shim_call("sqlite_r_open", as.character(path), as.integer(flags),
            as.integer(mode))
}

libsqlite_close <- function(db) {
  shim_call("sqlite_r_close", db)
}

# --- connection state and diagnostics -----------------------------------
# The shim's sqlite_r_connection dispatches by op: 0 is_readonly,
# 1 last_insert_rowid, 2 set_busy_timeout, 3 error_code,
# 4 error_code_extended, 5 error message. These three were missing from the
# R-facing surface, so no example could exercise them (bindings/CONTRACT.md
# section 8). error_message returns the library's static data.
set_busy_timeout <- function(db, ms) {
  as.integer(shim_call("sqlite_r_connection", db, 2L, as.integer(ms)))
}

error_code <- function(db) {
  as.integer(shim_call("sqlite_r_connection", db, 3L, 0L))
}

error_code_extended <- function(db) {
  as.integer(shim_call("sqlite_r_connection", db, 4L, 0L))
}

error_message <- function(db) {
  as.character(shim_call("sqlite_r_connection", db, 5L, 0L))
}

# --- statement execution (kind 0 command, 1 transaction, 2 non_query,
#     3 scalar, 4 reader) -----------------------------------------------
execute <- function(db, sql, kind = 0L, params = NULL, txexpected = 0L) {
  if (!is.null(params) && !is.list(params)) params <- list(params)
  shim_call("sqlite_r_execute", db, as.character(sql), as.integer(kind),
            params, as.integer(txexpected))
}

execute_command <- function(db, sql) execute(db, sql, kind = 0L)
execute_transaction <- function(db, sql, txexpected) {
  execute(db, sql, kind = 1L, txexpected = as.integer(txexpected))
}
execute_non_query <- function(db, sql, params = NULL) {
  execute(db, sql, kind = 2L, params = params)
}
execute_scalar <- function(db, sql, params = NULL) {
  execute(db, sql, kind = 3L, params = params)
}
execute_reader <- function(db, sql, params = NULL) {
  execute(db, sql, kind = 4L, params = params)
}

begin_transaction <- function(db) {
  execute(db, "BEGIN IMMEDIATE", kind = 1L, txexpected = 1L)
}
commit_transaction <- function(db) {
  execute(db, "COMMIT", kind = 1L, txexpected = 0L)
}
rollback_transaction <- function(db) {
  execute(db, "ROLLBACK", kind = 1L, txexpected = 0L)
}

# --- json ---------------------------------------------------------------
reader_to_json <- function(db, sql, params = NULL) {
  shim_call("sqlite_r_json", db, 0L, as.character(sql), NULL, params)
}

# The same JSON document built through sqlite_core_json_writer_callback
# (the specialized row callback of the frozen API).
reader_json_callback <- function(db, sql, params = NULL) {
  shim_call("sqlite_r_json", db, 2L, as.character(sql), NULL, params)
}

insert_json <- function(db, table, json) {
  shim_call("sqlite_r_json", db, 1L, as.character(table), as.character(json), NULL)
}

# --- blob fields --------------------------------------------------------
get_blob <- function(db, sql) {
  shim_call("sqlite_r_get_blob", db, as.character(sql))
}

insert_blob <- function(db, table, column, rowid, file, chunk = 0L) {
  shim_call("sqlite_r_write_blob", db, 0L, as.character(table),
            as.character(column), as.numeric(rowid), as.character(file),
            as.integer(chunk))
}

update_blob <- function(db, table, column, rowid, file, chunk = 0L) {
  shim_call("sqlite_r_write_blob", db, 1L, as.character(table),
            as.character(column), as.numeric(rowid), as.character(file),
            as.integer(chunk))
}

# --- connection info / errors -------------------------------------------
# op 0 is_readonly, 1 last_insert_rowid, 2 set_busy_timeout(arg),
#    3 error_code, 4 error_code_extended, 5 error_message
connection_info <- function(db, op, arg = 0L) {
  shim_call("sqlite_r_connection", db, as.integer(op), arg)
}

is_readonly <- function(db) connection_info(db, 0L)
last_insert_rowid <- function(db) connection_info(db, 1L)
set_busy_timeout <- function(db, ms) connection_info(db, 2L, as.integer(ms))
error_code <- function(db) connection_info(db, 3L)
error_code_extended <- function(db) connection_info(db, 4L)
error_message <- function(db) connection_info(db, 5L)

library_version <- function() shim_call("sqlite_r_message", 0L)
sqlite_version <- function() shim_call("sqlite_r_message", 1L)

# --- string helpers (results are freed inside the shim) -----------------
# op 0 string_format(fmt, arg), 1 concat, 2 ansi_to_utf8, 3 utf8_to_ansi
string_op <- function(op, a, b = "") {
  shim_call("sqlite_r_string", as.integer(op), as.character(a),
            as.character(b))$value
}
string_format <- function(fmt, ...) {
  args <- list(...)
  arg <- if (length(args) == 0) "" else as.character(args[[1]])
  string_op(0L, fmt, arg)
}
string_concat <- function(a, b) string_op(1L, a, b)
ansi_to_utf8 <- function(a) string_op(2L, a)
utf8_to_ansi <- function(a) string_op(3L, a)
param_clean_prefix <- function(name) {
  shim_call("sqlite_r_param_clean_prefix", as.character(name))
}