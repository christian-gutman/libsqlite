#include "sqlite_core.h"

// Returns the library version (e.g. "1.0.0").
const char *sqlite_core_version()
{
    return SQLITE_CORE_VERSION_STR;
}

// Returns the version of the embedded SQLite engine (e.g. "3.54.1").
const char *sqlite_core_sqlite_version()
{
    return SQLITE_CORE_SQLITE_VERSION_STR;
}

   
