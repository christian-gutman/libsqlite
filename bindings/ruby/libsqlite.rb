# libsqlite3 binding for Ruby (ffi gem).
#
# Complete surface of the frozen public API: the 34 symbols exported by
# src/sqlite/core/sqlite.def. One dependency: the `ffi` gem
# (gem install ffi).
#
# Why ffi and not Fiddle (the standard library): Fiddle::Closure, the way
# to turn a Ruby block into a C function pointer, is broken on Ruby 3.x —
# calling the resulting pointer raises "undefined method 'call' for an
# instance of Fiddle::Closure" (Fiddle::Closure stopped working as a
# trampoline). The ffi gem has first-class callbacks, so the binding uses it.
#
# The one place that needs care is sqlite_core_param_t: it is a struct with a
# union that holds **native pointers** (name, blob_or_text.ptr), so the
# binding builds the struct in raw memory (ParamSet) and keeps the strings
# alive for the duration of the call.

require "ffi"

module Libsqlite
  extend FFI::Library

  SQLITE_OK = 0
  SQLITE_ERROR = 1
  SQLITE_MISUSE = 21
  SQLITE_CANTOPEN = 14

  BUFFER_TOO_SMALL = -2
  BLOB_TOO_SMALL = -3

  TYPE_NULL = 0
  TYPE_INT = 2
  TYPE_DOUBLE = 4
  TYPE_TEXT = 6
  TYPE_BLOB = 8

  DIRECTION_INPUT = 1
  DIRECTION_OUTPUT = 2
  DIRECTION_INPUTOUTPUT = 3

  LIB_CANDIDATES = [
    "libsqlite3.so",
    "libsqlite3.dylib",
    "../../bin/x64-Linux/shared/Release/libsqlite3.so",
    "../../bin/x64-Windows/shared/Release/libsqlite3.dll",
    "../../bin/arm64-Darwin/shared/Release/libsqlite3.dylib",
  ].freeze

  # Layout of sqlite_core_param_t: name (pointer), the three ints, then the
  # union aligned to the pointer size (dVal / iVal share 8 bytes on LP64;
  # blob_or_text is ptr + len + max_len).
  # FFI::Platform::ADDRESS_SIZE is in bits (64), so the pointer size in bytes
  # is ADDRESS_SIZE / 8: 8 on LP64, 4 on ILP32.
  PTR = FFI::Platform::ADDRESS_SIZE / 8
  VALUE_OFFSET = PTR == 8 ? 24 : 16
  PARAM_SIZE = PTR == 8 ? 40 : 28
  # Mirror of sqlite_core_json_builder_t: { char*; size_t; size_t; int; int }
  BUILDER_SIZE = PTR == 8 ? 32 : 20

  class << self
    attr_reader :path
  end

  # Loads the library and binds the frozen surface (idempotent).
  def self.load(path = nil)
    return self if @ready

    path ||= LIB_CANDIDATES.find { |candidate| File.exist?(candidate) }
    raise "libsqlite3 library not found; pass the path explicitly" if path.nil?
    raise "libsqlite3 library not found: #{path}" unless File.exist?(path)

    ffi_lib File.expand_path(path)
    attach_functions
    @path = path
    @ready = true
    self
  end

  # Raw address of one frozen symbol: used to pass C function pointers
  # (sqlite_core_json_writer_callback to sqlite_core_execute_reader).
  def self.address(name)
    return nil if @path.nil?

    symbol = FFI::DynamicLibrary.open(@path, FFI::DynamicLibrary::RTLD_LAZY).find_symbol(name)
    symbol.nil? || symbol.null? ? nil : symbol.address
  end

  # Every entry point of the frozen surface, bound by name with its exact
  # signature (checked against src/sqlite/core/sqlite.def).
  def self.attach_functions
    # sqlite_core_row_callback_fn(stmt, columns, hasrows, ctx): declared
    # before the entry point that uses it as a parameter.
    callback :row_callback, [:pointer, :int, :int, :pointer], :int

    attach_function :version, :sqlite_core_version, [], :string
    attach_function :sqlite_version, :sqlite_core_sqlite_version, [], :string
    attach_function :error_code, :sqlite_core_error_code, [:pointer], :int
    attach_function :error_code_extended, :sqlite_core_error_code_extended, [:pointer], :int
    attach_function :error_message, :sqlite_core_error_message, [:pointer], :string

    attach_function :string_format, :sqlite_core_string_format, [:string, :string], :pointer
    attach_function :string_concat, :sqlite_core_string_concat, [:string, :string], :pointer
    attach_function :string_free, :sqlite_core_string_free, [:pointer], :void
    attach_function :utf8_to_ansi, :sqlite_core_string_utf8_to_ansi, [:string], :pointer
    attach_function :ansi_to_utf8, :sqlite_core_string_ansi_to_utf8, [:string], :pointer

    attach_function :open, :sqlite_core_open, [:string, :pointer], :pointer
    attach_function :open_ex, :sqlite_core_open_ex, [:string, :int, :pointer], :pointer
    attach_function :open_readonly, :sqlite_core_open_readonly, [:string, :pointer], :pointer
    attach_function :open_or_create, :sqlite_core_open_or_create, [:string, :pointer], :pointer
    attach_function :open_uri_readonly, :sqlite_core_open_uri_readonly, [:string, :pointer], :pointer
    attach_function :close, :sqlite_core_close, [:pointer], :int
    attach_function :is_readonly, :sqlite_core_is_readonly, [:pointer], :int
    attach_function :last_insert_rowid, :sqlite_core_last_insert_rowid, [:pointer], :int64
    attach_function :set_busy_timeout, :sqlite_core_set_busy_timeout, [:pointer, :int], :int

    attach_function :param_clean_prefix, :sqlite_core_param_clean_prefix, [:string], :string

    attach_function :execute_command, :sqlite_core_execute_command,
                    [:pointer, :string, :pointer, :int], :int
    attach_function :execute_transaction, :sqlite_core_execute_transaction,
                    [:pointer, :string, :int, :pointer, :int], :int
    attach_function :begin_transaction, :sqlite_core_begin_transaction,
                    [:pointer, :pointer, :int], :int
    attach_function :commit_transaction, :sqlite_core_commit_transaction,
                    [:pointer, :pointer, :int], :int
    attach_function :rollback_transaction, :sqlite_core_rollback_transaction,
                    [:pointer, :pointer, :int], :int
    attach_function :execute_non_query, :sqlite_core_execute_non_query,
                    [:pointer, :string, :pointer, :int, :pointer, :int], :int
    attach_function :execute_scalar, :sqlite_core_execute_scalar,
                    [:pointer, :string, :pointer, :int, :pointer, :pointer, :int], :int
    attach_function :execute_reader, :sqlite_core_execute_reader,
                    [:pointer, :string, :pointer, :int, :row_callback, :pointer, :pointer, :int], :int

    attach_function :reader_to_json, :sqlite_core_execute_reader_to_json,
                    [:pointer, :string, :pointer, :int, :pointer, :pointer, :int], :int
    attach_function :insert_json, :sqlite_core_insert_json,
                    [:pointer, :string, :string, :pointer, :int], :int

    attach_function :get_blob, :sqlite_core_get_blob,
                    [:pointer, :string, :pointer, :int, :pointer], :int
    attach_function :insert_blob, :sqlite_core_insert_blob,
                    [:pointer, :string, :string, :int64, :string, :int, :pointer, :pointer, :int], :int
    attach_function :update_blob, :sqlite_core_update_blob,
                    [:pointer, :string, :string, :int64, :string, :int, :pointer, :pointer, :int], :int
  end

  # --- Ruby-facing helpers ----------------------------------------------

  def self.c_string(value)
    bytes = value.to_s.b
    pointer = FFI::MemoryPointer.new(:char, bytes.bytesize + 1)
    pointer.put_bytes(0, bytes + 0.chr)
    pointer
  end

  def self.err_buffer
    buffer = FFI::MemoryPointer.new(:char, 512)
    buffer.put_bytes(0, 0.chr * 512)
    buffer
  end

  def self.err_text(buffer)
    text = buffer.read_string(512)
    text.to_s.sub(/\0.*\z/m, "")
  end

  def self.int_out
    buffer = FFI::MemoryPointer.new(:int)
    buffer.put_int32(0, 0)
    buffer
  end

  def self.read_int(buffer)
    buffer.get_int32(0)
  end

  def self.long_out
    buffer = FFI::MemoryPointer.new(:int64)
    buffer.put_int64(0, 0)
    buffer
  end

  def self.read_long(buffer)
    buffer.get_int64(0)
  end

  def self.pointer_out
    buffer = FFI::MemoryPointer.new(:pointer)
    buffer.put_pointer(0, nil)
    buffer
  end

  def self.read_pointer(buffer)
    pointer = buffer.read_pointer
    pointer.nil? || pointer.null? ? 0 : pointer.address
  end

  def self.read_pointer_at(buffer, offset)
    pointer = buffer.get_pointer(offset)
    pointer.nil? || pointer.null? ? 0 : pointer.address
  end

  # Reads a char* owned by the library's static data: never free it (the
  # :string return type of ffi already copies it).
  def self.static_string(value)
    value.to_s
  end

  # Reads a char* allocated by the SQLite allocator and releases it with
  # sqlite_core_string_free (never the Ruby GC).
  def self.take_string(pointer)
    # Accepts an FFI::Pointer or a raw address (Integer).
    ptr = pointer.is_a?(Integer) ? FFI::Pointer.new(pointer) : pointer
    return nil if ptr.nil? || ptr.null?

    value = ptr.read_string
    string_free(ptr)
    value
  end

  # Zeroed sqlite_core_param_t, used as the out_scalar_result of
  # sqlite_core_execute_scalar.
  def self.param_struct
    struct = FFI::MemoryPointer.new(:char, PARAM_SIZE)
    struct.put_bytes(0, 0.chr * PARAM_SIZE)
    struct
  end
end