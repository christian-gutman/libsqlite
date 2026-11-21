#ifndef SQLITE_CORE_H
#define SQLITE_CORE_H
 
#ifdef __cplusplus 
extern "C" {     
#endif

// Library version (this connector wrapper). The SQLite engine version is
// exposed separately through SQLITE_CORE_SQLITE_VERSION_STR.
#define SQLITE_CORE_VERSION_MAJOR 1
#define SQLITE_CORE_VERSION_MINOR 0
#define SQLITE_CORE_VERSION_PATCH 0
#define SQLITE_CORE_REVISION 0
#define SQLITE_CORE_SUBREVISION 0
#define SQLITE_CORE_VERSION_STR "1.0.0"
#define SQLITE_CORE_VERNUM 0x10000

// Embedded engine version, taken from the vendored amalgamation.
#define SQLITE_CORE_SQLITE_VERSION_STR SQLITE_VERSION

#include "engine/sqlite3.h"

#include "compat/sqlite_compat.h"

CORE_API const char *SqlLite_Core_Version();

CORE_API const char *SqlLite_Core_SqliteVersion();
 
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
 
typedef enum {
    SQLITE_CORE_TYPE_NULL = 0,
    SQLITE_CORE_TYPE_INT = 2,
    SQLITE_CORE_TYPE_DOUBLE = 4,
    SQLITE_CORE_TYPE_TEXT = 6,
    SQLITE_CORE_TYPE_BLOB = 8
} SqlLiteCoreParamType;

typedef enum {
    SQLITE_CORE_DIRECTION_INPUT = 1,
    SQLITE_CORE_DIRECTION_OUTPUT = 2,
    SQLITE_CORE_DIRECTION_INPUTOUTPUT = 3
} SqlLiteCoreParameterDirection;

typedef struct {
    const char *name;
    int no_insensitive;
    SqlLiteCoreParamType type;
    SqlLiteCoreParameterDirection Direction;
    union {
        double dVal;
        sqlite3_int64 iVal;
        struct {
            char *ptr;
            int len;
            int max_len;
        } blob_or_text;
    } value;
} SqlLiteCoreParam;

typedef int(*SqlLiteCoreRowCallback)(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);


#include "sqlite_core_error.h"
#include "sqlite_core_string.h"
#include "sqlite_core_cnx.h"
#include "sqlite_core_parameter.h"
#include "sqlite_core_command.h"
#include "sqlite_core_transaction.h"
#include "sqlite_core_builder.h"
#include "sqlite_core_field.h"

#ifdef __cplusplus
}
#endif 

 
#endif // SQLITE_CORE_H
