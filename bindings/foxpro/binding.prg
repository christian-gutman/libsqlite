* binding.prg — the 34 frozen declarations of libsqlite3.dll for
* Visual FoxPro 9 (see bindings/README.md).
*
* The example (example_full.prg) re-declares the subset it uses; keep this
* file as the single list: bindings/verify_symbols.py checks it against
* src/sqlite/core/sqlite.def.

LPARAMETERS tcDll
LOCAL lcDll
lcDll = IIF(Empty(tcDll) OR VARTYPE(tcDll) != 'C' OR Empty(tcDll), "libsqlite3.dll", tcDll)

* ---- errors ----------------------------------------------------------
DECLARE INTEGER sqlite_core_error_code          IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_error_code_extended IN (lcDll) INTEGER @db
DECLARE STRING   sqlite_core_error_message      IN (lcDll) INTEGER @db

* ---- strings ---------------------------------------------------------
DECLARE STRING   sqlite_core_string_format      IN (lcDll) STRING @fmt
DECLARE STRING   sqlite_core_string_concat      IN (lcDll) STRING @a, STRING @b
DECLARE void     sqlite_core_string_free        IN (lcDll) INTEGER @str
DECLARE STRING   sqlite_core_string_utf8_to_ansi IN (lcDll) STRING @s
DECLARE STRING   sqlite_core_string_ansi_to_utf8 IN (lcDll) STRING @s

* ---- connections -----------------------------------------------------
DECLARE INTEGER sqlite_core_open                IN (lcDll) STRING @path, INTEGER @rc
DECLARE INTEGER sqlite_core_open_ex             IN (lcDll) STRING @path, INTEGER @flags, INTEGER @rc
DECLARE INTEGER sqlite_core_open_readonly       IN (lcDll) STRING @path, INTEGER @rc
DECLARE INTEGER sqlite_core_open_or_create      IN (lcDll) STRING @path, INTEGER @rc
DECLARE INTEGER sqlite_core_open_uri_readonly   IN (lcDll) STRING @path, INTEGER @rc
DECLARE INTEGER sqlite_core_close               IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_is_readonly         IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_last_insert_rowid   IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_set_busy_timeout    IN (lcDll) INTEGER @db, INTEGER @ms

* ---- parameters ------------------------------------------------------
DECLARE INTEGER sqlite_core_param_clean_prefix  IN (lcDll) STRING @name

* ---- commands --------------------------------------------------------
DECLARE INTEGER sqlite_core_execute_command     IN (lcDll) INTEGER @db, STRING @sql, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_transaction IN (lcDll) INTEGER @db, STRING @sql, INTEGER @tx, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_non_query   IN (lcDll) INTEGER @db, STRING @sql, INTEGER @params, INTEGER @count, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_scalar      IN (lcDll) INTEGER @db, STRING @sql, INTEGER @params, INTEGER @count, INTEGER @out, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_reader      IN (lcDll) INTEGER @db, STRING @sql, INTEGER @params, INTEGER @count, INTEGER @cb, INTEGER @ctx, STRING @err, INTEGER @max

* ---- transactions ----------------------------------------------------
DECLARE INTEGER sqlite_core_begin_transaction   IN (lcDll) INTEGER @db, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_commit_transaction  IN (lcDll) INTEGER @db, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_rollback_transaction IN (lcDll) INTEGER @db, STRING @err, INTEGER @max

* ---- version ---------------------------------------------------------
DECLARE STRING   sqlite_core_version            IN (lcDll)
DECLARE STRING   sqlite_core_sqlite_version     IN (lcDll)

* ---- json ------------------------------------------------------------
DECLARE INTEGER sqlite_core_json_writer_callback IN (lcDll) INTEGER @stmt, INTEGER @columns, INTEGER @hasrows, INTEGER @ctx
DECLARE INTEGER sqlite_core_execute_reader_to_json IN (lcDll) INTEGER @db, STRING @sql, INTEGER @params, INTEGER @count, INTEGER @json, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_insert_json         IN (lcDll) INTEGER @db, STRING @table, STRING @json, STRING @err, INTEGER @max

* ---- fields ----------------------------------------------------------
DECLARE INTEGER sqlite_core_get_blob            IN (lcDll) INTEGER @db, STRING @sql, STRING @dest, INTEGER @max, INTEGER @copied
DECLARE INTEGER sqlite_core_insert_blob         IN (lcDll) INTEGER @db, STRING @table, STRING @column, INTEGER @rowid, STRING @file, INTEGER @chunk, INTEGER @written, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_update_blob         IN (lcDll) INTEGER @db, STRING @table, STRING @column, INTEGER @rowid, STRING @file, INTEGER @chunk, INTEGER @written, STRING @err, INTEGER @max