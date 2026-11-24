# libsqlite3 binding for Julia.
#
# Complete surface of the frozen public API: the 34 symbols exported by
# src/sqlite/core/sqlite.def. No packages and no compilation: ccall goes
# straight to the built library (libsqlite3.dll / libsqlite3.so /
# libsqlite3.dylib).
#
# Two Julia specifics are handled explicitly:
#   * sqlite_core_param_t holds *native pointers* inside its union
#     (name, blob_or_text.ptr), so ParamSet builds the struct array in raw
#     memory (offsets hardcoded per pointer size, like the Ruby binding);
#   * sqlite_core_execute_reader needs a C function pointer: Julia builds
#     one with @cfunction, and the callback state lives in a module-global
#     REF_STATE (a closure cannot be used there).

module Libsqlite

using Libdl

export SQLITE_OK, SQLITE_ERROR, SQLITE_MISUSE, SQLITE_CANTOPEN,
       BUFFER_TOO_SMALL, BLOB_TOO_SMALL,
       TYPE_NULL, TYPE_INT, TYPE_DOUBLE, TYPE_TEXT, TYPE_BLOB,
       ParamSet, load!, open_or_create, open_readonly, open_or_create_uri, open_ex,
       command, transaction_command, begin_transaction, commit_transaction,
       rollback_transaction, non_query, scalar_int, scalar_text,
       reader_count, reader_json, json_writer_callback, insert_json,
       get_blob, insert_blob, update_blob, is_readonly, last_insert_rowid,
       set_busy_timeout, error_code, error_message, param_clean_prefix,
       library_version, sqlite_version, string_concat, string_format,
       ansi_to_utf8, utf8_to_ansi, take_string, static_string

const SQLITE_OK = 0
const SQLITE_ERROR = 1
const SQLITE_MISUSE = 21
const SQLITE_CANTOPEN = 14

const BUFFER_TOO_SMALL = -2
const BLOB_TOO_SMALL = -3

const TYPE_NULL = 0
const TYPE_INT = 2
const TYPE_DOUBLE = 4
const TYPE_TEXT = 6
const TYPE_BLOB = 8

# Layout of sqlite_core_param_t: name (pointer), the three ints, then the
# union aligned to the pointer size (dVal / iVal share 8 bytes on LP64;
# blob_or_text is ptr + len + max_len).
# Integer division: Julia uses ÷ (\ is matrix left division, which returns
# 64 \ 8 = 0.125).
const PTR = Sys.WORD_SIZE ÷ 8            # 8 on LP64, 4 on ILP32
const VALUE_OFFSET = PTR == 8 ? 24 : 16
const PARAM_SIZE = PTR == 8 ? 40 : 28

const LIB_CANDIDATES = [
    "libsqlite3.so",
    "libsqlite3.dylib",
    "../../bin/x64-Linux/shared/Release/libsqlite3.so",
    "../../bin/x64-Windows/shared/Release/libsqlite3.dll",
    "../../bin/arm64-Darwin/shared/Release/libsqlite3.dylib",
]

const _LIB = Ref{String}()
const _DLL = Ref{Ptr{Cvoid}}(C_NULL)

# --- loading -----------------------------------------------------------

"""Loads the library (explicit path or one of the candidates)."""
function load!(path = nothing)
    if path === nothing
        path = LIB_CANDIDATES[findfirst(isfile, LIB_CANDIDATES)]
        if path === nothing
            error("libsqlite3 library not found; pass the path explicitly")
        end
    elseif !isfile(path)
        error("libsqlite3 library not found: ", path)
    end
    _LIB[] = abspath(path)
    # RTLD_GLOBAL: the ccall name-only form resolves symbols in the current
    # process, so the library must publish its symbols (Julia's default
    # dlopen flags do not).
    _DLL[] = Libdl.dlopen(_LIB[], Libdl.RTLD_LAZY | Libdl.RTLD_GLOBAL)
    _LIB[]
end

# --- strings -----------------------------------------------------------

"""Reads a char* owned by the library's static data: never free it."""
static_string(ptr::Ptr{Cchar}) = unsafe_string(ptr)

"""Reads a char* allocated by the SQLite allocator and releases it with
sqlite_core_string_free (never the Julia GC)."""
function take_string(ptr::Ptr{Cchar})
    ptr == C_NULL && return nothing
    value = unsafe_string(ptr)
    ccall((:sqlite_core_string_free,), Cvoid, (Ptr{Cchar},), ptr)
    value
end

function err_buffer()
    zeros(UInt8, 512)
end

function err_text(buffer::Vector{UInt8})
    end_index = findfirst(==(0x00), buffer)
    end_index === nothing && return ""
    String(view(buffer, 1:end_index - 1))
end

# --- parameters --------------------------------------------------------

"""Named parameters. sqlite_core_param_t holds native pointers, so the
struct array is built in raw memory and the strings are owned here."""
mutable struct ParamSet
    rows::Vector{Tuple{Symbol, String, Any}}
    owned::Vector{Vector{UInt8}}

    ParamSet() = new(Tuple{Symbol, String, Any}[], Vector{UInt8}[])
end

text(p::ParamSet, name::AbstractString, value::AbstractString) =
    push!(p.rows, (:text, String(name), String(value)))
integer(p::ParamSet, name::AbstractString, value::Integer) =
    push!(p.rows, (:int, String(name), convert(Int64, value)))
floating(p::ParamSet, name::AbstractString, value::Real) =
    push!(p.rows, (:double, String(name), convert(Float64, value)))
null_value(p::ParamSet, name::AbstractString) =
    push!(p.rows, (:null, String(name), nothing))

# Builds the contiguous array and returns [pointer, count]; a C_NULL pointer
# is returned when the set is empty.
function array_pointer(p::ParamSet)
    isempty(p.rows) && return (C_NULL, 0)
    bytes = zeros(UInt8, PARAM_SIZE * length(p.rows))
    empty!(p.owned)
    for (index, (kind, name, value)) in enumerate(p.rows)
        base = (index - 1) * PARAM_SIZE
        name_ptr = own_string!(p, name)
        store_pointer(bytes, base, name_ptr)
        store_int32(bytes, base + PTR, 0)                       # no_insensitive
        store_int32(bytes, base + PTR + 4, type_code(kind))
        store_int32(bytes, base + PTR + 8, 1)                   # INPUT
        fill_value(bytes, base + VALUE_OFFSET, kind, value, p)
    end
    pointer = Base.pointer(bytes)
    push!(p.owned, bytes)
    (pointer, length(p.rows))
end

type_code(kind) = kind === :int ? TYPE_INT :
                  kind === :double ? TYPE_DOUBLE :
                  kind === :text ? TYPE_TEXT : TYPE_NULL

function fill_value(bytes, offset, kind, value, p::ParamSet)
    if kind === :int
        store_int64(bytes, offset, value)
    elseif kind === :double
        store_double(bytes, offset, value)
    elseif kind === :text
        store_pointer(bytes, offset, own_string!(p, value))
        store_int32(bytes, offset + PTR, -1)                    # NUL-terminated
        store_int32(bytes, offset + PTR + 4, 0)                 # max_len
    end
    nothing
end

function own_string!(p::ParamSet, value)
    buffer = Vector{UInt8}(undef, sizeof(value) + 1)
    copyto!(buffer, 1, Vector{UInt8}(value), 1, sizeof(value))
    buffer[end] = 0
    push!(p.owned, buffer)
    pointer(buffer)
end

# --- raw memory helpers ------------------------------------------------
# reinterpret from the *signed* types: UInt32(Int32(-1)) would raise
# InexactError, and two's complement is what the C side reads.

store_pointer(bytes, offset, address) =
    bytes[offset + 1:offset + PTR] = reinterpret(UInt8, [UInt(address)])
store_int32(bytes, offset, value) =
    bytes[offset + 1:offset + 4] = reinterpret(UInt8, [Int32(value)])
store_int64(bytes, offset, value) =
    bytes[offset + 1:offset + 8] = reinterpret(UInt8, [Int64(value)])
store_double(bytes, offset, value) =
    bytes[offset + 1:offset + 8] = reinterpret(UInt8, [Float64(value)])

load_int32(bytes, offset) = Int32(reinterpret(UInt32, bytes[offset + 1:offset + 4])[1])
load_int64(bytes, offset) = Int64(reinterpret(UInt64, bytes[offset + 1:offset + 8])[1])
load_pointer(bytes, offset) =
    UInt(reinterpret(PTR == 8 ? UInt64 : UInt32, bytes[offset + 1:offset + PTR])[1])

# --- the 34 frozen symbols, wrapped ------------------------------------

"""sqlite_core_version / sqlite_core_sqlite_version (static strings)."""
library_version() = unsafe_string(ccall((:sqlite_core_version,), Ptr{Cchar}, ()))
sqlite_version() = unsafe_string(ccall((:sqlite_core_sqlite_version,), Ptr{Cchar}, ()))

function version_pair()
    (library_version(), sqlite_version())
end

# error / connection state
error_code(db) = ccall((:sqlite_core_error_code,), Cint, (Ptr{Cvoid},), db)
error_code_extended(db) = ccall((:sqlite_core_error_code_extended,), Cint,
                                (Ptr{Cvoid},), db)
error_message(db) = unsafe_string(ccall((:sqlite_core_error_message,),
                                        Ptr{Cchar}, (Ptr{Cvoid},), db))
is_readonly(db) = ccall((:sqlite_core_is_readonly,), Cint, (Ptr{Cvoid},), db)
last_insert_rowid(db) = ccall((:sqlite_core_last_insert_rowid,), Int64,
                              (Ptr{Cvoid},), db)
set_busy_timeout(db, ms) = ccall((:sqlite_core_set_busy_timeout,), Cint,
                                  (Ptr{Cvoid}, Cint), db, ms)

# string helpers (heap results are read and released here)
string_concat(a, b) = take_string(ccall((:sqlite_core_string_concat,),
                                        Ptr{Cchar}, (Cstring, Cstring), a, b))
string_format(fmt, arg) = take_string(ccall((:sqlite_core_string_format,),
                                            Ptr{Cchar}, (Cstring, Cstring), fmt, arg))
param_clean_prefix(name) = unsafe_string(
    ccall((:sqlite_core_param_clean_prefix,), Ptr{Cchar}, (Cstring,), name))
utf8_to_ansi(text) = take_string(ccall((:sqlite_core_string_utf8_to_ansi,),
                                       Ptr{Cchar}, (Cstring,), text))
ansi_to_utf8(text) = take_string(ccall((:sqlite_core_string_ansi_to_utf8,),
                                       Ptr{Cchar}, (Cstring,), text))

# open / close
function open_or_create(path)
    rc = Ref{Cint}(0)
    db = ccall((:sqlite_core_open_or_create,), Ptr{Cvoid},
               (Cstring, Ref{Cint}), path, rc)
    (db, rc[])
end

function open_ex(path, flags)
    rc = Ref{Cint}(0)
    db = ccall((:sqlite_core_open_ex,), Ptr{Cvoid},
               (Cstring, Cint, Ref{Cint}), path, flags, rc)
    (db, rc[])
end

function open_or_create_uri(path)
    rc = Ref{Cint}(0)
    db = ccall((:sqlite_core_open_uri_readonly,), Ptr{Cvoid},
               (Cstring, Ref{Cint}), path, rc)
    (db, rc[])
end

function open_readonly(path)
    rc = Ref{Cint}(0)
    db = ccall((:sqlite_core_open_readonly,), Ptr{Cvoid},
               (Cstring, Ref{Cint}), path, rc)
    (db, rc[])
end

close(db) = ccall((:sqlite_core_close,), Cint, (Ptr{Cvoid},), db)

# execution
function command(db, sql)
    err = err_buffer()
    rc = ccall((:sqlite_core_execute_command,), Cint,
               (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint), db, sql, err, length(err))
    (rc, err_text(err))
end

function transaction_command(db, sql, txexpected)
    err = err_buffer()
    rc = ccall((:sqlite_core_execute_transaction,), Cint,
               (Ptr{Cvoid}, Cstring, Cint, Ptr{Cvoid}, Cint),
               db, sql, txexpected, err, length(err))
    (rc, err_text(err))
end

begin_transaction(db) = ccall((:sqlite_core_begin_transaction,), Cint,
                              (Ptr{Cvoid}, Ptr{Cvoid}, Cint), db, err_buffer(), 512)
commit_transaction(db) = ccall((:sqlite_core_commit_transaction,), Cint,
                               (Ptr{Cvoid}, Ptr{Cvoid}, Cint), db, err_buffer(), 512)
rollback_transaction(db) = ccall((:sqlite_core_rollback_transaction,), Cint,
                                 (Ptr{Cvoid}, Ptr{Cvoid}, Cint), db, err_buffer(), 512)

function non_query(db, sql, params::ParamSet = ParamSet())
    err = err_buffer()
    ptr, count = array_pointer(params)
    rc = ccall((:sqlite_core_execute_non_query,), Cint,
               (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ptr{Cvoid}, Cint),
               db, sql, ptr, count, err, length(err))
    (rc, err_text(err))
end

function _scalar(db, sql, params::ParamSet)
    err = err_buffer()
    out = zeros(UInt8, PARAM_SIZE)
    ptr, count = array_pointer(params)
    rc = ccall((:sqlite_core_execute_scalar,), Cint,
               (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ptr{Cvoid}, Ptr{Cvoid}, Cint),
               db, sql, ptr, count, out, err, length(err))
    (rc, out)
end

"""sqlite_core_execute_scalar for INTEGER results."""
function scalar_int(db, sql, params::ParamSet = ParamSet())
    rc, out = _scalar(db, sql, params)
    rc == SQLITE_OK || return nothing
    load_int32(out, PTR + 4) == TYPE_INT || return nothing
    load_int64(out, VALUE_OFFSET)
end

"""sqlite_core_execute_scalar for TEXT results: the destination buffer has
to be pre-filled (blob_or_text.ptr + max_len)."""
function scalar_text(db, sql, params::ParamSet = ParamSet())
    err = err_buffer()
    out = zeros(UInt8, PARAM_SIZE)
    buffer = zeros(UInt8, 512)
    store_pointer(out, VALUE_OFFSET, Base.pointer(buffer))
    store_int32(out, VALUE_OFFSET + PTR, 0)
    store_int32(out, VALUE_OFFSET + PTR + 4, length(buffer))
    ptr, count = array_pointer(params)
    rc = ccall((:sqlite_core_execute_scalar,), Cint,
               (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ptr{Cvoid}, Ptr{Cvoid}, Cint),
               db, sql, ptr, count, out, err, length(err))
    type = load_int32(out, PTR + 4)
    end_index = findfirst(==(0x00), buffer)
    value = (type == TYPE_TEXT && rc == SQLITE_OK && end_index !== nothing) ?
            String(view(buffer, 1:end_index - 1)) : ""
    (rc, value, type)
end

# reader: a Julia function is wrapped with @cfunction. The callback state
# lives in the module as Refs (a closure cannot be used with @cfunction).
const REF_STATE = (rows = Ref(0), columns = Ref(0))

function count_rows(stmt, columns, hasrows, ctx)::Cint
    if hasrows == 1
        REF_STATE.rows[] += 1
        REF_STATE.columns[] = columns
    end
    Cint(0)
end

const COUNT_CALLBACK = Ref{Ptr{Cvoid}}(C_NULL)

"""sqlite_core_execute_reader: one call of the callback per row (hasrows ==
0 marks an empty result set). Returns the row count or a negative code."""
function reader_count(db, sql, params::ParamSet = ParamSet())
    err = err_buffer()
    REF_STATE.rows[] = 0
    REF_STATE.columns[] = 0
    if COUNT_CALLBACK[] == C_NULL
        COUNT_CALLBACK[] = @cfunction(count_rows, Cint,
                                      (Ptr{Cvoid}, Cint, Cint, Ptr{Cvoid}))
    end
    ptr, count = array_pointer(params)
    rc = ccall((:sqlite_core_execute_reader,), Cint,
               (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ptr{Cvoid}, Ptr{Cvoid},
                Ptr{Cvoid}, Cint),
               db, sql, ptr, count, COUNT_CALLBACK[], C_NULL, err, length(err))
    (rc, REF_STATE.rows[], REF_STATE.columns[])
end

# json
function reader_json(db, sql, params::ParamSet = ParamSet())
    err = err_buffer()
    out = Ref{Ptr{Cchar}}(C_NULL)
    ptr, count = array_pointer(params)
    rows = ccall((:sqlite_core_execute_reader_to_json,), Cint,
                 (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ref{Ptr{Cchar}},
                  Ptr{Cvoid}, Cint),
                 db, sql, ptr, count, out, err, length(err))
    (rows, take_string(out[]))
end

function insert_json(db, table, json)
    err = err_buffer()
    rows = ccall((:sqlite_core_insert_json,), Cint,
                 (Ptr{Cvoid}, Cstring, Cstring, Ptr{Cvoid}, Cint),
                 db, table, json, err, length(err))
    (rows, err_text(err))
end

"""The same JSON document built by driving sqlite_core_execute_reader with
sqlite_core_json_writer_callback and a builder context (mirror of
sqlite_core_json_builder_t)."""
function json_writer_callback(db, sql, params::ParamSet = ParamSet())
    err = err_buffer()
    builder = zeros(UInt8, PTR == 8 ? 32 : 20)
    ptr, count = array_pointer(params)
    callback = Libdl.dlsym(_DLL[], :sqlite_core_json_writer_callback)
    rows = ccall((:sqlite_core_execute_reader,), Cint,
                 (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ptr{Cvoid}, Ptr{Cvoid},
                  Ptr{Cvoid}, Cint),
                 db, sql, ptr, count, callback, Base.pointer(builder), err, length(err))
    body = take_string(Ptr{Cchar}(load_pointer(builder, 0)))
    (rows, body)
end

# blob fields
function get_blob(db, sql)
    err = err_buffer()
    capacity = 4096
    buffer = Vector{UInt8}(undef, capacity)
    while true
        buffer .= 0x00
        copied = Ref{Cint}(0)
        rc = ccall((:sqlite_core_get_blob,), Cint,
                   (Ptr{Cvoid}, Cstring, Ptr{Cvoid}, Cint, Ref{Cint}),
                   db, sql, buffer, capacity, copied)
        if rc == BUFFER_TOO_SMALL && capacity < 64 * 1024 * 1024
            capacity *= 2
            buffer = Vector{UInt8}(undef, capacity)
            continue
        end
        return (rc, buffer[1:copied[]], copied[])
    end
end

function _insert_blob(db, table, column, rowid, file, chunk)
    err = err_buffer()
    written = Ref{Cint}(0)
    rc = ccall((:sqlite_core_insert_blob,), Cint,
               (Ptr{Cvoid}, Cstring, Cstring, Int64, Cstring, Cint,
                Ref{Cint}, Ptr{Cvoid}, Cint),
               db, table, column, rowid, file, chunk, written, err, length(err))
    (rc, written[], err_text(err))
end

function _update_blob(db, table, column, rowid, file, chunk)
    err = err_buffer()
    written = Ref{Cint}(0)
    rc = ccall((:sqlite_core_update_blob,), Cint,
               (Ptr{Cvoid}, Cstring, Cstring, Int64, Cstring, Cint,
                Ref{Cint}, Ptr{Cvoid}, Cint),
               db, table, column, rowid, file, chunk, written, err, length(err))
    (rc, written[], err_text(err))
end

insert_blob(db, table, column, rowid, file, chunk = 0) =
    _insert_blob(db, table, column, rowid, file, chunk)
update_blob(db, table, column, rowid, file, chunk = 0) =
    _update_blob(db, table, column, rowid, file, chunk)

end # module