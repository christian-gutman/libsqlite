/*
 * libsqlite.prg — the 34 frozen symbols of libsqlite3.dll for the
 * Clipper family, as thin runtime wrappers.
 *
 * Harbour / xHarbour : wrappers call hb_libLoad("libsqlite3.dll") once
 *                      and then hb_libDo() per symbol.
 * NAM (Clipper 5.x)  : the FnLoad() variant uses _dllfn().
 *
 * bindings/verify_symbols.py checks this file against sqlite.def.
 */

#ifdef __HARBOUR__
#define NAM_STYLE .F.
#else
#define NAM_STYLE .T.
#endif

STATIC s_hLib := NIL

FUNCTION LibHandle()
   IF s_hLib == NIL
#ifdef __HARBOUR__
      s_hLib := hb_libLoad("libsqlite3.dll")
#else
      s_hLib := "libsqlite3.dll"
#endif
   ENDIF
RETURN s_hLib

/* --- errors ------------------------------------------------------- */
FUNCTION SqlErrorCode(db)        ; RETURN FnCall("sqlite_core_error_code", db)
FUNCTION SqlErrorCodeExt(db)     ; RETURN FnCall("sqlite_core_error_code_extended", db)
FUNCTION SqlErrorMessage(db)     ; RETURN FnCall("sqlite_core_error_message", db)

/* --- strings ------------------------------------------------------ */
FUNCTION SqlStringFormat(f)      ; RETURN FnCall("sqlite_core_string_format", f)
FUNCTION SqlStringConcat(a, b)   ; RETURN FnCall("sqlite_core_string_concat", a, b)
FUNCTION SqlStringFree(s)        ; RETURN FnCallVoid("sqlite_core_string_free", s)
FUNCTION SqlUtf8ToAnsi(s)        ; RETURN FnCall("sqlite_core_string_utf8_to_ansi", s)
FUNCTION SqlAnsiToUtf8(s)        ; RETURN FnCall("sqlite_core_string_ansi_to_utf8", s)

/* --- connections -------------------------------------------------- */
FUNCTION SqlOpen(path, rc)       ; RETURN FnCall("sqlite_core_open", path, @rc)
FUNCTION SqlOpenEx(path, fl, rc) ; RETURN FnCall("sqlite_core_open_ex", path, fl, @rc)
FUNCTION SqlOpenReadOnly(p, rc)  ; RETURN FnCall("sqlite_core_open_readonly", p, @rc)
FUNCTION SqlOpenOrCreate(p, rc)  ; RETURN FnCall("sqlite_core_open_or_create", p, @rc)
FUNCTION SqlOpenUriReadOnly(p, rc) ; RETURN FnCall("sqlite_core_open_uri_readonly", p, @rc)
FUNCTION SqlClose(db)            ; RETURN FnCall("sqlite_core_close", db)
FUNCTION SqlIsReadOnly(db)       ; RETURN FnCall("sqlite_core_is_readonly", db)
FUNCTION SqlLastInsertRowid(db)  ; RETURN FnCall("sqlite_core_last_insert_rowid", db)
FUNCTION SqlSetBusyTimeout(db, ms) ; RETURN FnCall("sqlite_core_set_busy_timeout", db, ms)

/* --- parameters --------------------------------------------------- */
FUNCTION SqlParamCleanPrefix(n)  ; RETURN FnCall("sqlite_core_param_clean_prefix", n)

/* --- commands ------------------------------------------------------ */
FUNCTION SqlExecuteCommand(db, sql, err, max)      ; RETURN FnCall("sqlite_core_execute_command", db, sql, err, max)
FUNCTION SqlExecuteTransaction(db, sql, tx, e, m)  ; RETURN FnCall("sqlite_core_execute_transaction", db, sql, tx, e, m)
FUNCTION SqlExecuteNonQuery(db, sql, p, n, e, m)   ; RETURN FnCall("sqlite_core_execute_non_query", db, sql, p, n, e, m)
FUNCTION SqlExecuteScalar(db, sql, p, n, o, e, m)  ; RETURN FnCall("sqlite_core_execute_scalar", db, sql, p, n, o, e, m)
FUNCTION SqlExecuteReader(db, sql, p, n, cb, c, e, m) ; RETURN FnCall("sqlite_core_execute_reader", db, sql, p, n, cb, c, e, m)

/* --- transactions -------------------------------------------------- */
FUNCTION SqlBeginTransaction(db, e, m)    ; RETURN FnCall("sqlite_core_begin_transaction", db, e, m)
FUNCTION SqlCommitTransaction(db, e, m)   ; RETURN FnCall("sqlite_core_commit_transaction", db, e, m)
FUNCTION SqlRollbackTransaction(db, e, m) ; RETURN FnCall("sqlite_core_rollback_transaction", db, e, m)

/* --- version ------------------------------------------------------- */
FUNCTION SqlVersion()          ; RETURN FnCall("sqlite_core_version")
FUNCTION SqlSqliteVersion()    ; RETURN FnCall("sqlite_core_sqlite_version")

/* --- json ---------------------------------------------------------- */
FUNCTION SqlJsonWriterCallback(stmt, cols, has, ctx) ; RETURN FnCall("sqlite_core_json_writer_callback", stmt, cols, has, ctx)
FUNCTION SqlReaderToJson(db, sql, p, n, out, e, m)   ; RETURN FnCall("sqlite_core_execute_reader_to_json", db, sql, p, n, out, e, m)
FUNCTION SqlInsertJson(db, t, j, e, m)               ; RETURN FnCall("sqlite_core_insert_json", db, t, j, e, m)

/* --- fields -------------------------------------------------------- */
FUNCTION SqlGetBlob(db, sql, dest, max, copied)      ; RETURN FnCall("sqlite_core_get_blob", db, sql, dest, max, @copied)
FUNCTION SqlInsertBlob(db, t, c, rowid, file, ch, w, e, m) ; RETURN FnCall("sqlite_core_insert_blob", db, t, c, rowid, file, ch, @w, e, m)
FUNCTION SqlUpdateBlob(db, t, c, rowid, file, ch, w, e, m) ; RETURN FnCall("sqlite_core_update_blob", db, t, c, rowid, file, ch, @w, e, m)

/* --- generic callers ------------------------------------------------ */
FUNCTION FnCall(cName, ...)
#ifdef __HARBOUR__
RETURN hb_libDo(LibHandle(), cName, ...)
#else
RETURN CALLDLL(_dllfn(LibHandle(), cName), ...)
#endif
RETURN NIL

FUNCTION FnCallVoid(cName, ...)
   FnCall(cName, ...)
RETURN NIL