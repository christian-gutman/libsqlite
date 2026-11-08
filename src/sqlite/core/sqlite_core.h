#ifndef SQLITE_CORE_H
#define SQLITE_CORE_H
 
#ifdef __cplusplus 
extern "C" {     
#endif

#define SQLITE_CORE_VERSION_MAJOR 3
#define SQLITE_CORE_VERSION_MINOR 54
#define SQLITE_CORE_VERSION_PATCH 1
#define SQLITE_CORE_REVISION 1
#define SQLITE_CORE_SUBREVISION 0
#define SQLITE_CORE_VERSION_STR "3.54.1"
#define SQLITE_CORE_VERNUM 0x3541

#include "engine/sqlite3.h"

#include "core.h"
#include "core_encoding.h"

CORE_API const char *SqlLite_Core_Version();
 
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
        int iVal;
        struct {
            char *ptr;
            int len;
            int max_len;
        } blob_or_text;
    } value;
} SqlLiteCoreParam;

typedef int(CORE_CALL *SqlLiteCoreRowCallback)(
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

#ifdef __cplusplus
}
#endif 

 
#endif // SQLITE_CORE_H
