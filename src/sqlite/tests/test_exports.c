/*
 * Export consistency test.
 *
 * Verifies at runtime that every public function of the API is actually
 * exported by the shared library, so sqlite.def (Windows) and the symbol
 * visibility attributes (Linux/macOS) cannot drift from the headers.
 *
 * Usage: test_exports <path-to-shared-library>
 * (CMake passes $<TARGET_FILE:libsqlite3>; registered only for shared builds.)
 */

#include <stdio.h>

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

/* This list must match the SQLITE_CORE_API-declared functions in the public
 * headers (src/sqlite/core/sqlite_core*.h) and the entries in sqlite.def. */
static const char *kPublicSymbols[] = {
    /* errors */
    "sqlite_core_error_code",
    "sqlite_core_error_code_extended",
    "sqlite_core_error_message",
    /* strings */
    "sqlite_core_string_format",
    "sqlite_core_string_concat",
    "sqlite_core_string_free",
    "sqlite_core_string_utf8_to_ansi",
    "sqlite_core_string_ansi_to_utf8",
    /* connections */
    "sqlite_core_open_readonly",
    "sqlite_core_open_ex",
    "sqlite_core_open_or_create",
    "sqlite_core_open",
    "sqlite_core_open_uri_readonly",
    "sqlite_core_close",
    "sqlite_core_is_readonly",
    "sqlite_core_last_insert_rowid",
    "sqlite_core_set_busy_timeout",
    /* parameters */
    "sqlite_core_param_clean_prefix",
    /* commands */
    "sqlite_core_execute_transaction",
    "sqlite_core_execute_non_query",
    "sqlite_core_execute_scalar",
    "sqlite_core_execute_reader",
    "sqlite_core_execute_command",
    /* transactions */
    "sqlite_core_begin_transaction",
    "sqlite_core_commit_transaction",
    "sqlite_core_rollback_transaction",
    /* version */
    "sqlite_core_version",
    "sqlite_core_sqlite_version",
    /* json */
    "sqlite_core_json_writer_callback",
    "sqlite_core_execute_reader_to_json",
    "sqlite_core_insert_json",
    /* fields */
    "sqlite_core_get_blob",
    "sqlite_core_insert_blob",
    "sqlite_core_update_blob",
};

#define SYMBOL_COUNT (int)(sizeof(kPublicSymbols) / sizeof(kPublicSymbols[0]))

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: test_exports <path-to-shared-library>\n");
        return 2;
    }

    const char *lib_path = argv[1];
    int missing = 0;

#if defined(_WIN32)
    HMODULE lib = LoadLibraryA(lib_path);
    if (lib == NULL)
    {
        fprintf(stderr, "cannot load library: %s\n", lib_path);
        return 2;
    }

    printf("export check: %d symbols\n", SYMBOL_COUNT);
    for (int i = 0; i < SYMBOL_COUNT; i++)
    {
        FARPROC proc = GetProcAddress(lib, kPublicSymbols[i]);
        if (proc == NULL)
        {
            fprintf(stderr, "MISSING EXPORT: %s\n", kPublicSymbols[i]);
            missing++;
        }
    }
    FreeLibrary(lib);
#else
    void *lib = dlopen(lib_path, RTLD_NOW | RTLD_LOCAL);
    if (lib == NULL)
    {
        fprintf(stderr, "cannot load library: %s (%s)\n", lib_path, dlerror());
        return 2;
    }

    printf("export check: %d symbols\n", SYMBOL_COUNT);
    for (int i = 0; i < SYMBOL_COUNT; i++)
    {
        void *sym = dlsym(lib, kPublicSymbols[i]);
        if (sym == NULL)
        {
            fprintf(stderr, "MISSING EXPORT: %s\n", kPublicSymbols[i]);
            missing++;
        }
    }
    dlclose(lib);
#endif

    if (missing > 0)
    {
        fprintf(stderr, "%d of %d symbols are not exported\n", missing, SYMBOL_COUNT);
        return 1;
    }

    printf("ok - all %d public symbols are exported\n", SYMBOL_COUNT);
    return 0;
}