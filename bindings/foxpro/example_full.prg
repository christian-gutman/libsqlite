* Canonical libsqlite3 flow for Visual FoxPro 9 (see bindings/README.md).
* Win32 DLL loaded with DECLARE DLL; every call is checked.
* Exit code (FoxPro convention): 1 = ok, 2 = failed checks.

PUBLIC lcDll, lcDb, lcFixture, lcBig, lcErr, lnRc, lnChecks, lnFail
PUBLIC db, rc, Nrows, written, copied, rowid, seen
PUBLIC ARRAY aErrn[1], aSink[1], aProbe[1]

LOCAL lcDll
lcDll = "..\..\bin\x32-Windows\shared\Release\libsqlite3.dll"

lcDb    = "foxpro_demo.db"
lcFixture = "foxpro_fixture.bin"
lcBig   = "foxpro_oversize.bin"
lcErr   = REPLICATE(CHR(0), 256)

IF FILE(lcDb)      
 DELETE FILE (lcDb)
ENDIF 

IF FILE(lcFixture) 
 DELETE FILE (lcFixture)
ENDIF 

IF FILE(lcBig) 
 DELETE FILE (lcBig)
ENDIF 

= StrToFile(lcFixture, REPLICATE(CHR(0),31) + CHR(255) + REPLICATE(CHR(0), 992))  && 1024 bytes
= StrToFile(lcBig, REPLICATE(CHR(0), 2048))

lnChecks = 0
lnFail   = 0

* --- declarations: the frozen 34-symbol surface ------------------------
DECLARE INTEGER sqlite_core_version IN (lcDll)
DECLARE INTEGER sqlite_core_sqlite_version IN (lcDll)
DECLARE INTEGER sqlite_core_open_or_create IN (lcDll) STRING @file, INTEGER @rc
DECLARE INTEGER sqlite_core_open_readonly IN (lcDll) STRING @file, INTEGER @rc
DECLARE INTEGER sqlite_core_close IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_is_readonly IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_last_insert_rowid IN (lcDll) INTEGER @db
DECLARE INTEGER sqlite_core_set_busy_timeout IN (lcDll) INTEGER @db, INTEGER @ms
DECLARE INTEGER sqlite_core_param_clean_prefix IN (lcDll) STRING @name
DECLARE INTEGER sqlite_core_execute_command IN (lcDll) INTEGER @db, STRING @sql, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_non_query IN (lcDll) INTEGER @db, STRING @sql, ;
    INTEGER @params, INTEGER @count, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_scalar IN (lcDll) INTEGER @db, STRING @sql, ;
    INTEGER @params, INTEGER @count, INTEGER @out, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_reader IN (lcDll) INTEGER @db, STRING @sql, ;
    INTEGER @params, INTEGER @count, INTEGER @cb, INTEGER @ctx, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_begin_transaction IN (lcDll) INTEGER @db, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_commit_transaction IN (lcDll) INTEGER @db, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_rollback_transaction IN (lcDll) INTEGER @db, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_execute_reader_to_json IN (lcDll) INTEGER @db, STRING @sql, ;
    INTEGER @params, INTEGER @count, INTEGER @json, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_insert_json IN (lcDll) INTEGER @db, STRING @table, ;
    STRING @json, STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_get_blob IN (lcDll) INTEGER @db, STRING @sql, ;
    STRING @dest, INTEGER @max, INTEGER @copied
DECLARE INTEGER sqlite_core_insert_blob IN (lcDll) INTEGER @db, STRING @table, ;
    STRING @column, INTEGER @rowid, STRING @file, INTEGER @chunk, INTEGER @written, ;
    STRING @err, INTEGER @max
DECLARE INTEGER sqlite_core_update_blob IN (lcDll) INTEGER @db, STRING @table, ;
    STRING @column, INTEGER @rowid, STRING @file, INTEGER @chunk, INTEGER @written, ;
    STRING @err, INTEGER @max
DECLARE STRING   sqlite_core_string_free IN (lcDll) INTEGER @str

* --- the flow ------------------------------------------------------------
? "libsqlite3 Visual FoxPro binding"
? "  library " + SYS(0) + " (engine 3.54.1)"

rc = 0
db = sqlite_core_open_or_create(lcDb, @rc)
?db
?rc
=CheckWith(db != 0 AND rc = 0, "open_or_create")
IF db = 0 
 RETURN 2
EndIf 

lcErr = SPACE(0)
* FoxPro passes buffers as STRING @; use the helpers in binding.prg
lnRc = sqlite_core_execute_command(db, "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)", @lcErr, LEN(lcErr))
=CheckWith(lnRc = 0, "create table")

* named parameters: the parameter array is a C struct layout; see
* bindings/foxpro/binding.prg (ParamInit/ParamSet) for the memory layout.
DIMENSION aParams[2 * 6]   && 2 parameters x 6 fields
= ParamSet(@aParams, 0, "id",  2, 1, "I", 42)      && INTEGER value
= ParamSet(@aParams, 1, "name", 6, 1, "T", "Ada")  && TEXT value
Nrows = sqlite_core_execute_non_query(db, "INSERT INTO person(id, name) VALUES (:id, :name)", ;
        INTEGER(aParams[1]), 2, @lcErr, LEN(lcErr))
=CheckWith(Nrows = 1, "named parameter insert")

seen = 0
Nrows = sqlite_core_execute_reader(db, "SELECT id, name FROM person ORDER BY id", 0, 0, ;
        (OnRowCount(@seen)), 0, @lcErr, LEN(lcErr))
=CheckWith(Nrows = 0 AND seen = 1, "reader visits the row")

dimension aOut[6]
= ParamInit(@aOut)
lnRc = sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM person", 0, 0, ;
        INTEGER(aOut[1]), @lcErr, LEN(lcErr))
=CheckWith(lnRc = 0, "scalar executes")

dimension aJson[1]
aJson = 0
Nrows = sqlite_core_execute_reader_to_json(db, "SELECT id, name FROM person ORDER BY id", 0, 0, ;
        @aJson[1], @lcErr, LEN(lcErr))
=CheckWith(Nrows = 1, "reader to json")
IF aJson[1] != 0 
 sqlite_core_string_free(aJson[1])
EndIf 

Nrows = sqlite_core_insert_json(db, "person", ;
        '[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]', @lcErr, LEN(lcErr))
=CheckWith(Nrows = 2, "insert_json batch")

written = 0
=CheckWith(sqlite_core_insert_blob(db, "person", "data", 2, lcFixture, 0, @written, @lcErr, LEN(lcErr)) = 1024 ;
        AND written = 1024, "insert_blob streams the file")

copied = 0
aSink = REPLICATE(CHR(0), 1024)
=CheckWith(sqlite_core_get_blob(db, "SELECT data FROM person WHERE id = 2", @aSink, 1024, @copied) = 0 ;
        AND copied = 1024, "get_blob reads it back")
=CheckWith(LEFT(aSink, 3) = LEFT(lcFixture, 3), "byte-for-byte round trip")

=CheckWith(sqlite_core_last_insert_rowid(db) > 0, "last_insert_rowid")

=CheckWith(sqlite_core_commit_transaction(db, @lcErr, LEN(lcErr)) != 0, "commit without transaction fails")
=CheckWith(sqlite_core_begin_transaction(db, @lcErr, LEN(lcErr)) = 1, "begin")
sqlite_core_execute_non_query(db, "INSERT INTO person(name) VALUES ('Temp')", 0, 0, @lcErr, LEN(lcErr))
=CheckWith(sqlite_core_rollback_transaction(db, @lcErr, LEN(lcErr)) = 1, "rollback")
= ParamInit(@aOut)
sqlite_core_execute_scalar(db, "SELECT COUNT(*) FROM person", 0, 0, INTEGER(aOut[1]), @lcErr, LEN(lcErr))
=CheckWith(ParamGetInt(@aOut) = 3, "rollback discarded the row")

=CheckWith(sqlite_core_update_blob(db, "person", "data", 2, lcBig, 0, @written, @lcErr, LEN(lcErr)) = -3, ;
        "update_blob refuses to exceed the column")

rc = 0
=CheckWith(sqlite_core_open_readonly("missing.db", @rc) = 0 AND rc = 14, "open missing file fails")
=CheckWith(sqlite_core_execute_non_query(db, "SELECT 1; SELECT 2", 0, 0, @lcErr, LEN(lcErr)) = 21, ;
        "multi-statement rejected")

=CheckWith(sqlite_core_close(db) = 1, "close")


IF FILE(lcDb)      
 DELETE FILE (lcDb)
ENDIF 

IF FILE(lcFixture) 
 DELETE FILE (lcFixture)
ENDIF 

IF FILE(lcBig) 
 DELETE FILE (lcBig)
ENDIF 

? ""
? ALLTRIM(STR(lnChecks)) + " checks, " + ALLTRIM(STR(lnFail)) + " failures"
RETURN IIF(lnFail = 0, 1, 2)

PROCEDURE CheckWith(tlCond, tcMsg)
    lnChecks = lnChecks + 1
    IF tlCond 
     ?? "  ok   - " + tcMsg
    Else
     ?tcMsg
     ?lnFail 
     *lnFail = lnFail + 1 AND ?? "  FAIL - " + tcMsg
    EndIf  
ENDPROC

PROCEDURE OnRowCount(tnSeen)
    * callback stored address: increments and returns 0 (continue)
    STORE (tnSeen + 1) TO (tnSeen)
    RETURN 0
ENDPROC

FUNCTION ParamSet(taParams, tnIndex, tcName, tnType, tnDir, tcKind, tvValue)
    * Writes 6 consecutive doubles (strings) of sqlite_core_param_t into
    * the array at tnIndex*6: name, no_insensitive, type, Direction,
    * dVal/iVal (kind I) or blob_or_text (kind T).
    LOCAL n
    n = tnIndex * 6 + 1
    taParams[n]   = tcName
    taParams[n+1] = 0
    taParams[n+2] = tnType
    taParams[n+3] = tnDir
    IF tcKind == "I" 
     taParams[n+4] = tvValue && integer union slot
    EndIf 
    IF tcKind == "T" 
       taParams[n+5] = tvValue && text pointer slot
    EndIf 
    RETURN .T.
ENDFUNC

FUNCTION ParamInit(taParams)
    taParams[1] = 0
    RETURN .T.
ENDFUNC

FUNCTION ParamGetInt(taParams)
    RETURN taParams[5]   && iVal slot of the out parameter
ENDFUNC