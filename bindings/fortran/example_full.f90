! Canonical libsqlite3 flow in Fortran (see bindings/README.md).
!
! Usage: ./run.sh [path-to-libsqlite3]
!   gfortran -I . example_full.f90 -L <libdir> -l:libsqlite3.so -Wl,-rpath,<libdir> -o example
!   LD_LIBRARY_PATH=<libdir> ./example
! Exit code = number of failed checks.
!
! Every string the C side reads is copied into a caller-owned c_char buffer:
! c_loc() of a local of a subprogram would dangle as soon as it returns.

program example_full
    use libsqlite
    implicit none

    character(len=*), parameter :: db_file = "fortran_demo.db"
    character(len=*), parameter :: fixture = "fortran_fixture.bin"
    character(len=*), parameter :: oversize = "fortran_oversize.bin"

    integer(c_int) :: total = 0
    integer(c_int) :: failures = 0

    ! caller-owned native buffers (target: c_loc() needs the attribute)
    character(kind=c_char), target :: err_buf(512)
    character(kind=c_char), target :: create_buf(66), insert_buf(56)
    character(kind=c_char), target :: count_buf(24), select_buf(43), name_buf(40)
    character(kind=c_char), target :: temp_buf(64), multi_buf(19), blob_sql(36)
    character(kind=c_char), target :: db_name(3), param_name(5), value_ada(4)
    character(kind=c_char), target :: text_buf(512), builder(32)
    character(kind=c_char), target :: db2_name(20)

    type(param_t) :: params(2)
    type(param_t) :: scalar_out
    type(param_value_t) :: param_value
    type(c_ptr) :: db, db2, json_ptr
    type(c_ptr), pointer :: body_ptr
    integer(c_int) :: rc, rows, rc_blob, written, copied, rc_over, rc_missing
    integer(c_int64_t) :: rowid
    character(len=:), allocatable :: doc, text, err_text_value, body, inner
    character(len=1024), target :: fixture_data
    character(len=2048) :: oversize_data
    integer :: unit, i
    logical :: ok

    ! 1024 bytes with 0x00 every 256 positions, like every other binding
    do i = 1, 1024
        fixture_data(i:i) = achar(mod(i - 1, 256))
    end do
    open (newunit=unit, file=fixture, form='unformatted', access='stream', &
          status='replace', action='write')
    write (unit) fixture_data(1:1024)
    close (unit)

    print '(a)', "libsqlite3 Fortran binding"
    print '(a)', "  library "//trim(ptr_to_string(sqlite_core_version(), 64))// &
                 " / engine "//trim(ptr_to_string(sqlite_core_sqlite_version(), 64))

    ! 1. open_or_create
    rc = 0
    db = sqlite_core_open_or_create(trim(db_file)//c_null_char, rc)
    call check(rc == sqlite_ok .and. c_associated(db), "open_or_create")
    if (.not. c_associated(db)) then
        call report()
        call exit(failures)
    end if

    ! 2. DDL
    call to_cstr("CREATE TABLE person(id INTEGER PRIMARY KEY, name TEXT, data BLOB)", create_buf)
    rc = sqlite_core_execute_command(db, create_buf, err_buf, 512_c_int)
    call check(rc == sqlite_ok, "create table")

    ! 3. named parameters (the union of the C struct)
    call to_cstr(":id", db_name)
    call to_cstr(":name", param_name)
    call to_cstr("Ada", value_ada)
    call set_param(params(1), db_name, type_int, param_value_int(42_c_int64_t))
    param_value = param_value_text(c_loc(value_ada))
    call set_param(params(2), param_name, type_text, param_value)
    call to_cstr("INSERT INTO person(id, name) VALUES (:id, :name)", insert_buf)
    rc = sqlite_core_execute_non_query(db, insert_buf, params, 2_c_int, err_buf, 512_c_int)
    call check(rc == 1, "named parameter insert")

    ! 4. reader: a Fortran function becomes the C callback (c_funloc)
    cb_rows = 0
    cb_columns = 0
    call to_cstr("SELECT id, name FROM person ORDER BY id", select_buf)
    rc = sqlite_core_execute_reader(db, select_buf, params, 0_c_int, &
                                    c_funloc(count_rows_cb), c_null_ptr, err_buf, 512_c_int)
    call check(rc == 1 .and. cb_rows == 1 .and. cb_columns == 2, "reader visits the row")

    ! 5. scalar (INTEGER)
    call to_cstr("SELECT COUNT(*) FROM person", count_buf)
    rc = sqlite_core_execute_scalar(db, count_buf, params, 0_c_int, scalar_out, err_buf, 512_c_int)
    call check(rc == sqlite_ok, "scalar counts rows")
    call check(transfer(scalar_out%value%addr, 0_c_int64_t) == 1_c_int64_t, "scalar value is 1")

    ! 5b. scalar as text (the C out struct doubles as the output, so a text
    ! result needs a destination buffer: blob_or_text.ptr + max_len)
    call to_cstr("SELECT name FROM person WHERE id = 42", name_buf)
    scalar_out%ftype = type_text
    scalar_out%value%addr = c_loc(text_buf)
    scalar_out%value%len = 0
    scalar_out%value%max_len = 512
    rc = sqlite_core_execute_scalar(db, name_buf, params, 0_c_int, scalar_out, err_buf, 512_c_int)
    call check(rc == sqlite_ok .and. scalar_out%ftype == type_text .and. &
               trim(ptr_to_string(scalar_out%value%addr, 512)) == "Ada", "scalar reads text")

    ! 6. reader to json
    json_ptr = c_null_ptr
    rows = sqlite_core_execute_reader_to_json(db, select_buf, params, 0_c_int, &
                                              json_ptr, err_buf, 512_c_int)
    doc = take_string(json_ptr)
    call check(rows == 1 .and. len(doc) > 0 .and. doc(1:1) == "[", "reader to json")
    print '(a)', "  json: "//trim(doc)

    ! 6b. same rows through sqlite_core_json_writer_callback: the builder
    ! context mirrors sqlite_core_json_builder_t (offset 0 is string_ptr) and
    ! the callback concatenates the row objects, while reader_to_json adds the
    ! array delimiters
    builder = c_null_char
    rows = sqlite_core_execute_reader(db, select_buf, params, 0_c_int, &
                                      c_funloc(sqlite_core_json_writer_callback), &
                                      c_loc(builder), err_buf, 512_c_int)
    block
        type(c_ptr) :: builder_ptr
        builder_ptr = c_loc(builder)
        call c_f_pointer(builder_ptr, body_ptr)
    end block
    body = ptr_to_string(body_ptr, 512)
    inner = doc(3:len(doc) - 2)        ! between "[\n" and "\n]"
    call check(rows == 1 .and. trim(body) == trim(inner), &
               "json_writer_callback builds the rows")
    if (c_associated(body_ptr)) then
        call sqlite_core_string_free(body_ptr)     ! SQLite allocator
    end if

    ! 7. json in
    call to_cstr('[{"id": 2, "name": "Line1"}, {"id": 3, "name": "Line2"}]', temp_buf)
    rows = sqlite_core_insert_json(db, "person"//c_null_char, temp_buf, err_buf, 512_c_int)
    call check(rows == 2, "insert_json batch")

    ! 8. blob round trip (chunked streaming write + read back)
    written = 0
    rc_blob = sqlite_core_insert_blob(db, "person"//c_null_char, "data"//c_null_char, &
                                      2_c_int64_t, trim(fixture)//c_null_char, 0_c_int, &
                                      written, err_buf, 512_c_int)
    print '(a, i0, a, i0)', "  (insert_blob rc=", rc_blob, " written=", written, ")"
    call check(rc_blob == 1024 .and. written == 1024, "insert_blob streams the file")

    call to_cstr("SELECT data FROM person WHERE id = 2", blob_sql)
    copied = 0
    rc = sqlite_core_get_blob(db, blob_sql, c_loc(fixture_data), 1024_c_int, copied)
    call check(rc == sqlite_ok .and. copied == 1024, "get_blob reads it back")
    ok = .true.
    do i = 1, 1024
        if (fixture_data(i:i) /= achar(mod(i - 1, 256))) ok = .false.
    end do
    call check(ok, "byte-for-byte round trip")

    rowid = sqlite_core_last_insert_rowid(db)
    call check(rowid > 0, "last_insert_rowid")

    ! 9. transactions
    rc = sqlite_core_commit_transaction(db, err_buf, 512_c_int)
    call check(rc /= sqlite_ok, "commit without transaction fails")
    rc = sqlite_core_begin_transaction(db, err_buf, 512_c_int)
    call check(rc == 1, "begin")
    call to_cstr("INSERT INTO person(name) VALUES ('Temp')", temp_buf)
    rc = sqlite_core_execute_non_query(db, temp_buf, params, 0_c_int, err_buf, 512_c_int)
    rc = sqlite_core_rollback_transaction(db, err_buf, 512_c_int)
    call check(rc == 1, "rollback")
    rc = sqlite_core_execute_scalar(db, count_buf, params, 0_c_int, scalar_out, err_buf, 512_c_int)
    call check(transfer(scalar_out%value%addr, 0_c_int64_t) == 3_c_int64_t, &
               "rollback discarded the row")

    ! 10. oversize update is reported (column smaller than the file)
    do i = 1, 2048
        oversize_data(i:i) = achar(0)
    end do
    open (newunit=unit, file=oversize, form='unformatted', access='stream', &
          status='replace', action='write')
    write (unit) oversize_data(1:2048)
    close (unit)
    written = 0
    rc_over = sqlite_core_update_blob(db, "person"//c_null_char, "data"//c_null_char, &
                                      2_c_int64_t, trim(oversize)//c_null_char, 0_c_int, &
                                      written, err_buf, 512_c_int)
    call check(rc_over == blob_too_small, "update_blob refuses to exceed the column")
    open (newunit=unit, file=oversize, status='old', action='write')
    close (unit, status='delete')

    ! 11. error paths
    rc_missing = 0
    db2 = sqlite_core_open_readonly("missing.db"//c_null_char, rc_missing)
    call check(.not. c_associated(db2) .and. rc_missing == sqlite_cantopen, &
               "open missing file fails")
    call to_cstr("SELECT 1; SELECT 2", multi_buf)
    rc = sqlite_core_execute_non_query(db, multi_buf, params, 0_c_int, err_buf, 512_c_int)
    call check(rc == sqlite_misuse, "multi-statement rejected")

    ! 12. the rest of the frozen surface
    rc = sqlite_core_execute_command(db, &
         "CREATE TABLE person(id INTEGER PRIMARY KEY)"//c_null_char, err_buf, 512_c_int)
    call check(rc == sqlite_error, "duplicate create fails")
    err_text_value = err_text(err_buf)
    call check(len_trim(err_text_value) > 0, "error_message reports the last failure")
    call check(sqlite_core_is_readonly(db) == 0, "is_readonly false on a read-write handle")
    call check(sqlite_core_set_busy_timeout(db, 1000_c_int) == 1, "set_busy_timeout")
    text = static_string(sqlite_core_param_clean_prefix(":name"//c_null_char))
    call check(trim(text) == "name", "param_clean_prefix strips the prefix")
    call check(sqlite_core_error_code(db) >= 0, "error_code reports a code")
    call check(trim(take_string(sqlite_core_string_concat("lib"//c_null_char, &
               "sqlite"//c_null_char))) == "libsqlite", &
               "string_concat is released with string_free")
    call check(trim(take_string(sqlite_core_string_format("%s says hi"//c_null_char, &
               "fortran"//c_null_char))) == "fortran says hi", "string_format")
    call check(trim(take_string(sqlite_core_string_ansi_to_utf8("Ada"//c_null_char))) == "Ada", &
               "ansi_to_utf8 round trips ascii")
    call check(trim(take_string(sqlite_core_string_utf8_to_ansi("Ada"//c_null_char))) == "Ada", &
               "utf8_to_ansi round trips ascii")

    ! sqlite_core_open_ex with SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
    call to_cstr(trim(db_file)//"2", db2_name)
    rc = 0
    db2 = sqlite_core_open_ex(db2_name, 6_c_int, rc)
    call check(rc == sqlite_ok .and. c_associated(db2), &
               "open_ex creates with read-write|create flags")
    rc = sqlite_core_close(db2)
    open (newunit=unit, file=trim(db_file)//"2", status='old', action='write')
    close (unit, status='delete')

    ! 13. close
    rc = sqlite_core_close(db)
    call check(rc == 1, "close")

    open (newunit=unit, file=fixture, status='old', action='write')
    close (unit, status='delete')
    open (newunit=unit, file=trim(db_file), status='old', action='write')
    close (unit, status='delete')

    call report()
    call exit(failures)

contains

    subroutine check(cond, msg)
        logical, intent(in) :: cond
        character(len=*), intent(in) :: msg

        total = total + 1
        if (cond) then
            print '(a)', "  ok   - "//trim(msg)
        else
            print '(a)', "  FAIL - "//trim(msg)
            failures = failures + 1
        end if
    end subroutine check

    subroutine report()
        print '(a, i0, a, i0)', "", total, " checks, ", failures, " failures"
    end subroutine report

end program example_full