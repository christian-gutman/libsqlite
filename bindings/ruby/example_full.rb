# Canonical libsqlite3 flow in Ruby (see bindings/README.md).
#
# Usage: ruby example_full.rb [path-to-libsqlite3]
# Requires the ffi gem (gem install ffi).
# Exit code = number of failed checks.

require_relative "libsqlite"

DB = "ruby_demo.db"
FIXTURE = "ruby_fixture.bin"
OVERSIZE = "ruby_oversize.bin"

STATE = { total: 0, failures: 0 }

def check(cond, msg)
  STATE[:total] += 1
  puts (cond ? "  ok   - " : "  FAIL - ") + msg
  STATE[:failures] += 1 unless cond
end

# A sqlite_core_param_t[] array built in native memory: the struct's union
# holds native pointers (name and the text value), so ParamSet owns the
# strings and hands out a contiguous array for the duration of the call.
class ParamSet
  def initialize
    @rows = []
    @owned = []
  end

  def text(name, value)
    @rows << [:text, name.to_s, value.to_s]
    self
  end

  def integer(name, value)
    @rows << [:int, name.to_s, value.to_i]
    self
  end

  def floating(name, value)
    @rows << [:double, name.to_s, value.to_f]
    self
  end

  def null_value(name)
    @rows << [:null, name.to_s, nil]
    self
  end

  def count
    @rows.size
  end

  # Returns [FFI::MemoryPointer with the array, count]; nil pointer when
  # there are no parameters.
  def pointer
    return [nil, 0] if @rows.empty?

    array = FFI::MemoryPointer.new(:char, Libsqlite::PARAM_SIZE * @rows.size)
    array.put_bytes(0, 0.chr * (Libsqlite::PARAM_SIZE * @rows.size))
    @owned.clear
    @rows.each_with_index do |(kind, name, value), index|
      base = index * Libsqlite::PARAM_SIZE
      name_ptr = own_string(name)
      put_pointer(array, base, name_ptr)
      array.put_int32(base + Libsqlite::PTR, 0)               # no_insensitive
      array.put_int32(base + Libsqlite::PTR + 4, type_for(kind))
      array.put_int32(base + Libsqlite::PTR + 8, Libsqlite::DIRECTION_INPUT)
      fill_value(array, base + Libsqlite::VALUE_OFFSET, kind, value)
    end
    [array, @rows.size]
  end

  private

  def type_for(kind)
    case kind
    when :int then Libsqlite::TYPE_INT
    when :double then Libsqlite::TYPE_DOUBLE
    when :text then Libsqlite::TYPE_TEXT
    else Libsqlite::TYPE_NULL
    end
  end

  def fill_value(array, offset, kind, value)
    case kind
    when :int then array.put_int64(offset, value)
    when :double then array.put_double(offset, value)
    when :text
      put_pointer(array, offset, own_string(value))
      array.put_int32(offset + Libsqlite::PTR, -1)        # NUL-terminated
      array.put_int32(offset + Libsqlite::PTR + 4, 0)     # max_len
    end
  end

  def own_string(str)
    pointer = FFI::MemoryPointer.new(:char, str.bytesize + 1)
    pointer.put_bytes(0, str.b + 0.chr)
    @owned << pointer
    pointer
  end

  def put_pointer(array, offset, pointer)
    array.put_pointer(offset, pointer)
  end
end

def cstr(text)
  Libsqlite.c_string(text)
end

def err
  $err ||= Libsqlite.err_buffer
end

def main
  Libsqlite.load(ARGV[0])

  puts "libsqlite3 Ruby binding"
  puts "  library #{Libsqlite.version} / engine #{Libsqlite.sqlite_version}"

  [DB, FIXTURE, OVERSIZE].each { |f| File.delete(f) if File.exist?(f) }
  fixture = (0...1024).map { |i| (i & 0xFF).chr }.join.b
  File.binwrite(FIXTURE, fixture)

  # 1. open_or_create
  rc = Libsqlite.int_out
  db = Libsqlite.open_or_create(DB, rc)
  check(Libsqlite.read_int(rc) == Libsqlite::SQLITE_OK, "open_or_create")
  return STATE[:failures] if db.null?

  # 2. DDL
  check(Libsqlite.execute_command(db, "CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)",
                                  err, 512) == Libsqlite::SQLITE_OK,
        "create table")

  # 3. named parameters (the union of the C struct)
  params = ParamSet.new.integer("id", 42).text("name", "Ada")
  param_ptr, param_count = params.pointer
  check(Libsqlite.execute_non_query(db, "INSERT INTO person(id, name) VALUES (:id, :name)",
                                    param_ptr, param_count, err, 512) == 1,
        "named parameter insert")

  # 4. reader: a Ruby block becomes the C callback (ffi calls the block per
  # row; hasrows == 0 marks an empty result set)
  counter = [0, 0]
  row_callback = proc do |stmt, columns, hasrows, _ctx|
    if hasrows == 1
      counter[0] += 1
      counter[1] = columns
    end
    0
  end
  check(Libsqlite.execute_reader(db, "SELECT id, name FROM person ORDER BY id", nil, 0,
                                 row_callback, nil, err, 512) == 1 &&
        counter[0] == 1 && counter[1] == 2,
        "reader visits the row")

  # 5. scalar
  out = Libsqlite.param_struct
  check(Libsqlite.execute_scalar(db, "SELECT COUNT(*) FROM person", nil, 0, out, nil, 0) == Libsqlite::SQLITE_OK &&
        scalar_int(out) == 1,
        "scalar counts rows")

  # 5b. scalar as text (the C out struct doubles as the output, so a text
  # result needs a destination buffer)
  text_out = Libsqlite.param_struct
  buffer = FFI::MemoryPointer.new(:char, 512)
  buffer.put_bytes(0, 0.chr * 512)
  # blob_or_text: ptr at VALUE_OFFSET, len at +PTR (the library fills it),
  # max_len at +PTR+4 (the destination capacity, set by the caller).
  text_out.put_pointer(Libsqlite::VALUE_OFFSET, buffer)
  text_out.put_int32(Libsqlite::VALUE_OFFSET + Libsqlite::PTR, 0)
  text_out.put_int32(Libsqlite::VALUE_OFFSET + Libsqlite::PTR + 4, 512)
  rc_text = Libsqlite.execute_scalar(db, "SELECT name FROM person WHERE id = 42", nil, 0,
                                     text_out, nil, 0)
  type = struct_type(text_out)
  check(rc_text == Libsqlite::SQLITE_OK && type == Libsqlite::TYPE_TEXT &&
        buffer.read_string.force_encoding("UTF-8") == "Ada",
        "scalar reads text")

  # 6. reader to json
  out_json = Libsqlite.pointer_out
  rows = Libsqlite.reader_to_json(db, "SELECT id, name FROM person ORDER BY id", nil, 0,
                                   out_json, err, 512)
  doc = Libsqlite.take_string(Libsqlite.read_pointer(out_json))
  check(rows == 1 && !doc.nil? && doc.start_with?("["), "reader to json")
  puts "  json: #{doc}"

  # 6b. same rows through sqlite_core_json_writer_callback: the builder
  # context mirrors sqlite_core_json_builder_t (see the Swift binding) and
  # the callback concatenates the row objects, while reader_to_json adds the
  # array delimiters
  builder = FFI::MemoryPointer.new(:char, Libsqlite::BUILDER_SIZE)
  builder.put_bytes(0, 0.chr * Libsqlite::BUILDER_SIZE)
  rows2 = Libsqlite.execute_reader(db, "SELECT id, name FROM person ORDER BY id", nil, 0,
                                   FFI::Pointer.new(Libsqlite.address("sqlite_core_json_writer_callback")),
                                   builder, err, 512)
  body = Libsqlite.take_string(Libsqlite.read_pointer_at(builder, 0))
  inner = doc.sub(/\A\[\n/, "").sub(/\n\]\z/, "")
  check(rows2 == 1 && body == inner, "json_writer_callback builds the rows")

  # 7. json in
  check(Libsqlite.insert_json(db, "person",
                              '[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]',
                              err, 512) == 2,
        "insert_json batch")

  # 8. blob round trip (chunked streaming write + read back)
  written = Libsqlite.long_out
  rc_blob = Libsqlite.insert_blob(db, "person", "data", 2, FIXTURE, 0, written, err, 512)
  puts "  (insert_blob rc=#{rc_blob} written=#{Libsqlite.read_long(written)} err=#{Libsqlite.err_text(err).strip})"
  check(rc_blob == fixture.bytesize && Libsqlite.read_long(written) == fixture.bytesize,
        "insert_blob streams the file")

  data, copied = get_blob(db, "SELECT data FROM person WHERE id = 2")
  check(copied == fixture.bytesize && data == fixture, "get_blob reads it back")
  check(data == fixture, "byte-for-byte round trip")

  check(Libsqlite.last_insert_rowid(db) > 0, "last_insert_rowid")

  # 9. transactions
  check(Libsqlite.commit_transaction(db, err, 512) != Libsqlite::SQLITE_OK,
        "commit without transaction fails")
  check(Libsqlite.begin_transaction(db, err, 512) == 1, "begin")
  Libsqlite.execute_non_query(db, "INSERT INTO person(name) VALUES ('Temp')", nil, 0, err, 512)
  check(Libsqlite.rollback_transaction(db, err, 512) == 1, "rollback")
  out2 = Libsqlite.param_struct
  Libsqlite.execute_scalar(db, "SELECT COUNT(*) FROM person", nil, 0, out2, nil, 0)
  check(scalar_int(out2) == 3, "rollback discarded the row")

  # 10. oversize update is reported (column smaller than the file)
  File.binwrite(OVERSIZE, (0.chr * (fixture.bytesize * 2)).b)
  rc_over = Libsqlite.update_blob(db, "person", "data", 2, OVERSIZE, 0, written, err, 512)
  check(rc_over == Libsqlite::BLOB_TOO_SMALL, "update_blob refuses to exceed the column")
  File.delete(OVERSIZE) if File.exist?(OVERSIZE)

  # 11. error paths
  rc_missing = Libsqlite.int_out
  missing = Libsqlite.open_readonly("missing.db", rc_missing)
  check((missing.nil? || missing.null?) && Libsqlite.read_int(rc_missing) == Libsqlite::SQLITE_CANTOPEN,
        "open missing file fails")
  check(Libsqlite.execute_non_query(db, "SELECT 1; SELECT 2", nil, 0, err, 512) == Libsqlite::SQLITE_MISUSE,
        "multi-statement rejected")

  # 12. the rest of the frozen surface
  broken = Libsqlite.execute_command(db, "CREATE TABLE person(id INTEGER PRIMARY KEY)", err, 512)
  check(broken == Libsqlite::SQLITE_ERROR, "duplicate create fails")
  check(!Libsqlite.err_text(err).empty?, "error_message reports the last failure")
  check(Libsqlite.is_readonly(db) == 0, "is_readonly false on a read-write handle")
  check(Libsqlite.set_busy_timeout(db, 1000) == 1, "set_busy_timeout")
  check(Libsqlite.param_clean_prefix(":name") == "name", "param_clean_prefix strips the prefix")
  check(Libsqlite.error_code(db) >= 0, "error_code reports a code")
  check(Libsqlite.take_string(Libsqlite.string_concat("lib", "sqlite")) == "libsqlite",
        "string_concat is released with string_free")
  check(Libsqlite.take_string(Libsqlite.string_format("%s says hi", "ruby")) == "ruby says hi",
        "string_format")

  # 13. close
  check(Libsqlite.close(db) == 1, "close")

  [DB, FIXTURE].each { |f| File.delete(f) if File.exist?(f) }
  puts "\n#{STATE[:total]} checks, #{STATE[:failures]} failures"
  STATE[:failures]
end

# Reads an INTEGER scalar out of the out_scalar_result struct.
def scalar_int(out_struct)
  type = struct_type(out_struct)
  return nil unless type == Libsqlite::TYPE_INT

  out_struct.get_int64(Libsqlite::VALUE_OFFSET)
end

# sqlite_core_param_t:type lives right after no_insensitive (PTR + 4).
def struct_type(out_struct)
  out_struct.get_int32(Libsqlite::PTR + 4)
end

# sqlite_core_get_blob: the buffer grows on BUFFER_TOO_SMALL.
def get_blob(db, sql)
  capacity = 4096
  buffer = FFI::MemoryPointer.new(:char, capacity)
  loop do
    buffer.put_bytes(0, 0.chr * capacity)
    copied = Libsqlite.int_out
    rc = Libsqlite.get_blob(db, sql, buffer, capacity, copied)
    if rc == Libsqlite::BUFFER_TOO_SMALL && capacity < 64 * 1024 * 1024
      capacity *= 2
      buffer = FFI::MemoryPointer.new(:char, capacity)
      next
    end
    count = Libsqlite.read_int(copied)
    return ["", count] if rc != Libsqlite::SQLITE_OK || count <= 0

    return [buffer.read_string(count), count]
  end
end

exit(main)