"""libsqlite3 binding for Python (ctypes).

Complete surface of the frozen public API: the 34 symbols in
src/sqlite/core/sqlite.def. No compilation needed; point LIB_PATH at the
built library (libsqlite3.dll / libsqlite3.so / libsqlite3.dylib).
"""

import ctypes
import os
import sys

LIB_CANDIDATES = [
    "libsqlite3.dll",
    "libsqlite3.so",
    "libsqlite3.dylib",
    "../bin/x64-Windows/shared/Release/libsqlite3.dll",
    "../bin/x32-Windows/shared/Release/libsqlite3.dll",
    "../bin/x64-Linux/shared/Release/libsqlite3.so",
    "../bin/arm64-Darwin/shared/Release/libsqlite3.dylib",
]


def load(path=None):
    if path:
        return ctypes.CDLL(path)
    for cand in LIB_CANDIDATES:
        if os.path.exists(cand):
            return ctypes.CDLL(cand)
    raise OSError("libsqlite3 library not found; pass the path explicitly")


# --- types ---------------------------------------------------------------
SQLITE_OK = 0
SQLITE_ERROR = 1
SQLITE_MISUSE = 21
SQLITE_CANTOPEN = 14

SQLITE_CORE_TYPE_NULL = 0
SQLITE_CORE_TYPE_INT = 2
SQLITE_CORE_TYPE_DOUBLE = 4
SQLITE_CORE_TYPE_TEXT = 6
SQLITE_CORE_TYPE_BLOB = 8

SQLITE_CORE_DIRECTION_INPUT = 1
SQLITE_CORE_DIRECTION_OUTPUT = 2
SQLITE_CORE_DIRECTION_INPUTOUTPUT = 3

SQLITE_CORE_BUFFER_TOO_SMALL = -2
SQLITE_CORE_BLOB_TOO_SMALL = -3

SQLITE_CORE_API = ctypes.c_int


class _BlobOrText(ctypes.Structure):
    _fields_ = [
        ("ptr", ctypes.c_void_p),
        ("len", ctypes.c_int),
        ("max_len", ctypes.c_int),
    ]


class _ParamValue(ctypes.Union):
    _fields_ = [
        ("dVal", ctypes.c_double),
        ("iVal", ctypes.c_int64),
        ("blob_or_text", _BlobOrText),
    ]


class SqliteCoreParam(ctypes.Structure):
    """Mirror of sqlite_core_param_t (see bindings/README.md)."""

    _fields_ = [
        ("name", ctypes.c_char_p),
        ("no_insensitive", ctypes.c_int),
        ("type", ctypes.c_int),
        ("Direction", ctypes.c_int),
        ("value", _ParamValue),
    ]

    @staticmethod
    def text(name, value):
        p = SqliteCoreParam()
        p.name = name.encode()
        p.type = SQLITE_CORE_TYPE_TEXT
        p.Direction = SQLITE_CORE_DIRECTION_INPUT
        p.value.blob_or_text.ptr = ctypes.cast(
            ctypes.c_char_p(value.encode("utf-8")), ctypes.c_void_p
        )
        p.value.blob_or_text.len = -1
        return p

    @staticmethod
    def integer(name, value):
        p = SqliteCoreParam()
        p.name = name.encode()
        p.type = SQLITE_CORE_TYPE_INT
        p.Direction = SQLITE_CORE_DIRECTION_INPUT
        p.value.iVal = value
        return p


ROW_CALLBACK = ctypes.CFUNCTYPE(
    ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p
)


class Lib:
    """All 34 exported functions with their C signatures."""

    def __init__(self, path=None):
        self.lib = load(path)
        L = self.lib
        # version
        L.sqlite_core_version.restype = ctypes.c_char_p
        L.sqlite_core_sqlite_version.restype = ctypes.c_char_p
        # errors
        L.sqlite_core_error_code.argtypes = [ctypes.c_void_p]
        L.sqlite_core_error_code_extended.argtypes = [ctypes.c_void_p]
        L.sqlite_core_error_message.argtypes = [ctypes.c_void_p]
        L.sqlite_core_error_message.restype = ctypes.c_void_p
        # strings
        L.sqlite_core_string_format.restype = ctypes.c_void_p
        L.sqlite_core_string_concat.restype = ctypes.c_void_p
        L.sqlite_core_string_free.argtypes = [ctypes.c_void_p]
        L.sqlite_core_string_utf8_to_ansi.restype = ctypes.c_void_p
        L.sqlite_core_string_ansi_to_utf8.restype = ctypes.c_void_p
        # connections
        L.sqlite_core_open.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
        L.sqlite_core_open.restype = ctypes.c_void_p
        L.sqlite_core_open_ex.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
        L.sqlite_core_open_ex.restype = ctypes.c_void_p
        L.sqlite_core_open_readonly.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
        L.sqlite_core_open_readonly.restype = ctypes.c_void_p
        L.sqlite_core_open_or_create.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
        L.sqlite_core_open_or_create.restype = ctypes.c_void_p
        L.sqlite_core_open_uri_readonly.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int)]
        L.sqlite_core_open_uri_readonly.restype = ctypes.c_void_p
        L.sqlite_core_close.argtypes = [ctypes.c_void_p]
        L.sqlite_core_is_readonly.argtypes = [ctypes.c_void_p]
        L.sqlite_core_last_insert_rowid.argtypes = [ctypes.c_void_p]
        L.sqlite_core_last_insert_rowid.restype = ctypes.c_int64
        L.sqlite_core_set_busy_timeout.argtypes = [ctypes.c_void_p, ctypes.c_int]
        # parameters
        L.sqlite_core_param_clean_prefix.restype = ctypes.c_void_p
        L.sqlite_core_param_clean_prefix.argtypes = [ctypes.c_char_p]
        # commands
        L.sqlite_core_execute_command.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int
        ]
        L.sqlite_core_execute_transaction.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int
        ]
        L.sqlite_core_execute_non_query.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(SqliteCoreParam),
            ctypes.c_int, ctypes.c_char_p, ctypes.c_int,
        ]
        L.sqlite_core_execute_scalar.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(SqliteCoreParam), ctypes.c_int,
            ctypes.POINTER(SqliteCoreParam), ctypes.c_char_p, ctypes.c_int,
        ]
        L.sqlite_core_execute_reader.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(SqliteCoreParam), ctypes.c_int,
            ROW_CALLBACK, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int,
        ]
        # transactions
        L.sqlite_core_begin_transaction.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        L.sqlite_core_commit_transaction.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        L.sqlite_core_rollback_transaction.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
        # json
        L.sqlite_core_json_writer_callback.restype = ctypes.c_int
        L.sqlite_core_execute_reader_to_json.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(SqliteCoreParam), ctypes.c_int,
            ctypes.POINTER(ctypes.c_void_p), ctypes.c_char_p, ctypes.c_int,
        ]
        L.sqlite_core_insert_json.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int
        ]
        # fields
        L.sqlite_core_get_blob.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int,
            ctypes.POINTER(ctypes.c_int),
        ]
        ARG_BLOB = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int64,
            ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int),
            ctypes.c_char_p, ctypes.c_int,
        ]
        L.sqlite_core_insert_blob.argtypes = ARG_BLOB
        L.sqlite_core_update_blob.argtypes = ARG_BLOB

    # -- helpers ---------------------------------------------------------
    def string(self, ptr):
        """Reads a C string owned by the library's static data
        (version, error_message, param_clean_prefix): never free it."""
        if not ptr:
            return None
        return ctypes.string_at(ptr).decode("utf-8", "replace")

    def take(self, ptr):
        """Reads a heap string allocated by the SQLite allocator
        (string_format, string_concat, *ansi/utf8*, reader_to_json) and
        releases it with sqlite_core_string_free."""
        if not ptr:
            return None
        try:
            return ctypes.string_at(ptr).decode("utf-8", "replace")
        finally:
            self.lib.sqlite_core_string_free(ptr)

    def param_array(self, params):
        if not params:
            return None, 0
        arr = (SqliteCoreParam * len(params))(*params)
        return arr, len(params)