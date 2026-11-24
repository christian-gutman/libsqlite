! libsqlite3 binding for Fortran (iso_c_binding).
!
! Complete surface of the frozen public API: the 34 symbols in
! src/sqlite/core/sqlite.def. No compilation of the binding itself: link
! against libsqlite3 (libsqlite3.so / libsqlite3.dll / libsqlite3.dylib).
!
! The one place that needs care is sqlite_core_param_t: it is a struct with a
! union that holds *native pointers* (name, blob_or_text.ptr). Fortran has no
! union, and the C layout is:
!
!   const char *name;                8 bytes
!   int no_insensitive;              4
!   int type;                        4
!   int Direction;                   4  (+ 4 padding on LP64)
!   union { double dVal; sqlite3_int64 iVal;
!          struct { char *ptr; int len; int max_len; } blob_or_text; } value;
!                                 16 bytes
!
! The union's first 8 bytes are shared by dVal, iVal and blob_or_text.ptr, so a
! single bind(C) type with (addr, len, max_len) represents both cases:
! dVal/iVal are written with transfer() into addr, and text through c_loc().

module libsqlite
    use iso_c_binding, only: c_ptr, c_funptr, c_funloc, c_loc, c_associated, &
                             c_null_ptr, c_null_char, c_f_pointer, c_int, &
                             c_int64_t, c_double, c_char, c_size_t, c_long, &
                             c_float
    implicit none


    public :: sqlite3, sqlite3_stmt, param_value_t, param_t
    public :: sqlite_core_version, sqlite_core_sqlite_version
    public :: sqlite_core_error_code, sqlite_core_error_code_extended
    public :: sqlite_core_error_message
    public :: sqlite_core_string_format, sqlite_core_string_concat
    public :: sqlite_core_string_free, sqlite_core_string_utf8_to_ansi
    public :: sqlite_core_string_ansi_to_utf8
    public :: sqlite_core_open_ex, sqlite_core_open_readonly
    public :: sqlite_core_open_or_create, sqlite_core_open
    public :: sqlite_core_open_uri_readonly, sqlite_core_close
    public :: sqlite_core_is_readonly, sqlite_core_last_insert_rowid
    public :: sqlite_core_set_busy_timeout, sqlite_core_param_clean_prefix
    public :: sqlite_core_execute_command, sqlite_core_execute_transaction
    public :: sqlite_core_execute_non_query, sqlite_core_execute_scalar
    public :: sqlite_core_execute_reader, sqlite_core_begin_transaction
    public :: sqlite_core_commit_transaction, sqlite_core_rollback_transaction
    public :: sqlite_core_json_writer_callback
    public :: sqlite_core_execute_reader_to_json, sqlite_core_insert_json
    public :: sqlite_core_get_blob, sqlite_core_insert_blob
    public :: sqlite_core_update_blob
    public :: row_callback_fn, count_rows_cb, to_cstr, take_string, free_string
    public :: static_string, ptr_to_string, err_text, read_int, read_int64
    public :: param_value_int, param_value_double, param_value_text, set_param
    public :: sqlite_ok, sqlite_error, sqlite_misuse, sqlite_cantopen
    public :: buffer_too_small, blob_too_small, type_null, type_int
    public :: type_double, type_text, type_blob, direction_input

    ! return codes and frozen constants
    integer(c_int), parameter :: sqlite_ok = 0
    integer(c_int), parameter :: sqlite_error = 1
    integer(c_int), parameter :: sqlite_misuse = 21
    integer(c_int), parameter :: sqlite_cantopen = 14
    integer(c_int), parameter :: buffer_too_small = -2
    integer(c_int), parameter :: blob_too_small = -3
    integer(c_int), parameter :: type_null = 0
    integer(c_int), parameter :: type_int = 2
    integer(c_int), parameter :: type_double = 4
    integer(c_int), parameter :: type_text = 6
    integer(c_int), parameter :: type_blob = 8
    integer(c_int), parameter :: direction_input = 1

    ! reader trampoline counters (module scope, filled by count_rows_cb)
    integer(c_int) :: cb_rows = 0
    integer(c_int) :: cb_columns = 0

    ! opaque handles
    type :: sqlite3
        integer(c_int) :: unused
    end type sqlite3

    type :: sqlite3_stmt
        integer(c_int) :: unused
    end type sqlite3_stmt

    ! sqlite_core_row_callback_fn(stmt, columns, hasrows, ctx)
    abstract interface
        function row_callback_fn(stmt, columns, hasrows, ctx) bind(c)
            import :: c_ptr, c_char, c_int
            integer(c_int) :: row_callback_fn
            type(c_ptr), value :: stmt
            integer(c_int), value :: columns, hasrows
            type(c_ptr), value :: ctx
        end function row_callback_fn
    end interface

    ! The union, seen through the field it overlaps (see the header comment).
    type, bind(c) :: param_value_t
        type(c_ptr) :: addr
        integer(c_int) :: len
        integer(c_int) :: max_len
    end type param_value_t

    type, bind(c) :: param_t
        type(c_ptr) :: name
        integer(c_int) :: no_insensitive
        integer(c_int) :: ftype
        integer(c_int) :: direction
        type(param_value_t) :: value
    end type param_t


interface
    function sqlite_core_version() bind(c, name="sqlite_core_version")
        import :: c_ptr
        type(c_ptr) :: sqlite_core_version
    end function sqlite_core_version

    function sqlite_core_sqlite_version() bind(c, name="sqlite_core_sqlite_version")
        import :: c_ptr
        type(c_ptr) :: sqlite_core_sqlite_version
    end function sqlite_core_sqlite_version

    function sqlite_core_error_code(db) bind(c, name="sqlite_core_error_code")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_error_code
        type(c_ptr), value :: db
    end function sqlite_core_error_code

    function sqlite_core_error_code_extended(db) bind(c, name="sqlite_core_error_code_extended")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_error_code_extended
        type(c_ptr), value :: db
    end function sqlite_core_error_code_extended

    function sqlite_core_error_message(db) bind(c, name="sqlite_core_error_message")
        import :: c_ptr
        type(c_ptr) :: sqlite_core_error_message
        type(c_ptr), value :: db
    end function sqlite_core_error_message

    function sqlite_core_string_format(fmt, arg) bind(c, name="sqlite_core_string_format")
        import :: c_ptr, c_char
        type(c_ptr) :: sqlite_core_string_format
        character(kind=c_char), intent(in) :: fmt(*)
        character(kind=c_char), intent(in) :: arg(*)
    end function sqlite_core_string_format

    function sqlite_core_string_concat(a, b) bind(c, name="sqlite_core_string_concat")
        import :: c_ptr, c_char
        type(c_ptr) :: sqlite_core_string_concat
        character(kind=c_char), intent(in) :: a(*)
        character(kind=c_char), intent(in) :: b(*)
    end function sqlite_core_string_concat

    subroutine sqlite_core_string_free(ptr) bind(c, name="sqlite_core_string_free")
        import :: c_ptr
        type(c_ptr), value :: ptr
    end subroutine sqlite_core_string_free

    function sqlite_core_string_utf8_to_ansi(text) bind(c, name="sqlite_core_string_utf8_to_ansi")
        import :: c_ptr, c_char
        type(c_ptr) :: sqlite_core_string_utf8_to_ansi
        character(kind=c_char), intent(in) :: text(*)
    end function sqlite_core_string_utf8_to_ansi

    function sqlite_core_string_ansi_to_utf8(text) bind(c, name="sqlite_core_string_ansi_to_utf8")
        import :: c_ptr, c_char
        type(c_ptr) :: sqlite_core_string_ansi_to_utf8
        character(kind=c_char), intent(in) :: text(*)
    end function sqlite_core_string_ansi_to_utf8

    function sqlite_core_open_ex(path, flags, out_rc) bind(c, name="sqlite_core_open_ex")
        import :: c_ptr, c_char, c_int
        type(c_ptr) :: sqlite_core_open_ex
        character(kind=c_char), intent(in) :: path(*)
        integer(c_int), value :: flags
        integer(c_int), intent(out) :: out_rc
    end function sqlite_core_open_ex

    function sqlite_core_open_readonly(path, out_rc) bind(c, name="sqlite_core_open_readonly")
        import :: c_ptr, c_char, c_int
        type(c_ptr) :: sqlite_core_open_readonly
        character(kind=c_char), intent(in) :: path(*)
        integer(c_int), intent(out) :: out_rc
    end function sqlite_core_open_readonly

    function sqlite_core_open_or_create(path, out_rc) bind(c, name="sqlite_core_open_or_create")
        import :: c_ptr, c_char, c_int
        type(c_ptr) :: sqlite_core_open_or_create
        character(kind=c_char), intent(in) :: path(*)
        integer(c_int), intent(out) :: out_rc
    end function sqlite_core_open_or_create

    function sqlite_core_open(path, out_rc) bind(c, name="sqlite_core_open")
        import :: c_ptr, c_char, c_int
        type(c_ptr) :: sqlite_core_open
        character(kind=c_char), intent(in) :: path(*)
        integer(c_int), intent(out) :: out_rc
    end function sqlite_core_open

    function sqlite_core_open_uri_readonly(path, out_rc) bind(c, name="sqlite_core_open_uri_readonly")
        import :: c_ptr, c_char, c_int
        type(c_ptr) :: sqlite_core_open_uri_readonly
        character(kind=c_char), intent(in) :: path(*)
        integer(c_int), intent(out) :: out_rc
    end function sqlite_core_open_uri_readonly

    function sqlite_core_close(db) bind(c, name="sqlite_core_close")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_close
        type(c_ptr), value :: db
    end function sqlite_core_close

    function sqlite_core_is_readonly(db) bind(c, name="sqlite_core_is_readonly")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_is_readonly
        type(c_ptr), value :: db
    end function sqlite_core_is_readonly

    function sqlite_core_last_insert_rowid(db) bind(c, name="sqlite_core_last_insert_rowid")
        import :: c_ptr, c_int64_t
        integer(c_int64_t) :: sqlite_core_last_insert_rowid
        type(c_ptr), value :: db
    end function sqlite_core_last_insert_rowid

    function sqlite_core_set_busy_timeout(db, ms) bind(c, name="sqlite_core_set_busy_timeout")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_set_busy_timeout
        type(c_ptr), value :: db
        integer(c_int), value :: ms
    end function sqlite_core_set_busy_timeout

    function sqlite_core_param_clean_prefix(name) bind(c, name="sqlite_core_param_clean_prefix")
        import :: c_ptr, c_char
        type(c_ptr) :: sqlite_core_param_clean_prefix
        character(kind=c_char), intent(in) :: name(*)
    end function sqlite_core_param_clean_prefix

    function sqlite_core_execute_command(db, sql, err_buf, err_max) bind(c, name="sqlite_core_execute_command")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_execute_command
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_execute_command

    function sqlite_core_execute_transaction(db, sql, txexpected, err_buf, err_max) &
            bind(c, name="sqlite_core_execute_transaction")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_execute_transaction
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        integer(c_int), value :: txexpected
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_execute_transaction

    function sqlite_core_begin_transaction(db, err_buf, err_max) bind(c, name="sqlite_core_begin_transaction")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_begin_transaction
        type(c_ptr), value :: db
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_begin_transaction

    function sqlite_core_commit_transaction(db, err_buf, err_max) bind(c, name="sqlite_core_commit_transaction")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_commit_transaction
        type(c_ptr), value :: db
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_commit_transaction

    function sqlite_core_rollback_transaction(db, err_buf, err_max) bind(c, name="sqlite_core_rollback_transaction")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_rollback_transaction
        type(c_ptr), value :: db
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_rollback_transaction

    function sqlite_core_execute_non_query(db, sql, params, count, err_buf, err_max) &
            bind(c, name="sqlite_core_execute_non_query")
        import :: c_ptr, c_char, c_int, param_t
        integer(c_int) :: sqlite_core_execute_non_query
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        type(param_t), intent(in) :: params(*)
        integer(c_int), value :: count
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_execute_non_query

    function sqlite_core_execute_scalar(db, sql, params, count, out_scalar, err_buf, err_max) &
            bind(c, name="sqlite_core_execute_scalar")
        import :: c_ptr, c_char, c_int, param_t
        integer(c_int) :: sqlite_core_execute_scalar
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        type(param_t), intent(in) :: params(*)
        integer(c_int), value :: count
        type(param_t), intent(out) :: out_scalar
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_execute_scalar

    function sqlite_core_execute_reader(db, sql, params, count, cb, ctx, err_buf, err_max) &
            bind(c, name="sqlite_core_execute_reader")
        import :: c_ptr, c_char, c_int, c_funptr, param_t
        integer(c_int) :: sqlite_core_execute_reader
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        type(param_t), intent(in) :: params(*)
        integer(c_int), value :: count
        type(c_funptr), value :: cb
        type(c_ptr), value :: ctx
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_execute_reader

    function sqlite_core_json_writer_callback(stmt, columns, hasrows, ctx) &
            bind(c, name="sqlite_core_json_writer_callback")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_json_writer_callback
        type(c_ptr), value :: stmt
        integer(c_int), value :: columns, hasrows
        type(c_ptr), value :: ctx
    end function sqlite_core_json_writer_callback

    function sqlite_core_execute_reader_to_json(db, sql, params, count, out_json, err_buf, err_max) &
            bind(c, name="sqlite_core_execute_reader_to_json")
        import :: c_ptr, c_char, c_int, param_t
        integer(c_int) :: sqlite_core_execute_reader_to_json
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        type(param_t), intent(in) :: params(*)
        integer(c_int), value :: count
        type(c_ptr), intent(out) :: out_json
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_execute_reader_to_json

    function sqlite_core_insert_json(db, table_name, json_str, err_buf, err_max) &
            bind(c, name="sqlite_core_insert_json")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_insert_json
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: table_name(*)
        character(kind=c_char), intent(in) :: json_str(*)
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_insert_json

    function sqlite_core_get_blob(db, sql, dest, max_len, copied) bind(c, name="sqlite_core_get_blob")
        import :: c_ptr, c_char, c_int
        integer(c_int) :: sqlite_core_get_blob
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: sql(*)
        type(c_ptr), value :: dest
        integer(c_int), value :: max_len
        integer(c_int), intent(out) :: copied
    end function sqlite_core_get_blob

    function sqlite_core_insert_blob(db, table_name, column_name, rowid, file_path, chunk, &
                                     written, err_buf, err_max) &
            bind(c, name="sqlite_core_insert_blob")
        import :: c_ptr, c_char, c_int, c_int64_t
        integer(c_int) :: sqlite_core_insert_blob
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: table_name(*)
        character(kind=c_char), intent(in) :: column_name(*)
        integer(c_int64_t), value :: rowid
        character(kind=c_char), intent(in) :: file_path(*)
        integer(c_int), value :: chunk
        integer(c_int), intent(out) :: written
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_insert_blob

    function sqlite_core_update_blob(db, table_name, column_name, rowid, file_path, chunk, &
                                     written, err_buf, err_max) &
            bind(c, name="sqlite_core_update_blob")
        import :: c_ptr, c_char, c_int, c_int64_t
        integer(c_int) :: sqlite_core_update_blob
        type(c_ptr), value :: db
        character(kind=c_char), intent(in) :: table_name(*)
        character(kind=c_char), intent(in) :: column_name(*)
        integer(c_int64_t), value :: rowid
        character(kind=c_char), intent(in) :: file_path(*)
        integer(c_int), value :: chunk
        integer(c_int), intent(out) :: written
        character(kind=c_char), intent(out) :: err_buf(*)
        integer(c_int), value :: err_max
    end function sqlite_core_update_blob
end interface

contains

    ! --- helpers ---------------------------------------------------------

    ! Copies a Fortran string (plus the NUL terminator) into a caller-owned
    ! c_char buffer. c_loc() of a *local* buffer would dangle as soon as the
    ! subprogram returns, so the caller owns the memory for the whole call
    ! (this is why the example declares its own buffers).
    subroutine to_cstr(text, buf)
        character(len=*), intent(in) :: text
        character(kind=c_char), intent(out) :: buf(*)
        integer :: n, i

        n = len_trim(text)
        do i = 1, n
            buf(i) = text(i:i)
        end do
        buf(n + 1) = c_null_char
    end subroutine to_cstr

    function read_int(value) result(out)
        integer(c_int), intent(out) :: value
        integer(c_int) :: out
        out = value
    end function read_int

    function read_int64(value) result(out)
        integer(c_int64_t), intent(out) :: value
        integer(c_int64_t) :: out
        out = value
    end function read_int64

    ! Reads up to maxlen bytes of a C string into a Fortran string.
    function ptr_to_string(ptr, maxlen) result(text)
        type(c_ptr), intent(in) :: ptr
        integer, intent(in) :: maxlen
        character(len=:), allocatable :: text
        character(kind=c_char), pointer :: buffer(:)
        integer :: i, n

        if (.not. c_associated(ptr)) then
            text = ''
            return
        end if
        call c_f_pointer(ptr, buffer, [maxlen])
        n = 0
        do i = 1, maxlen
            if (buffer(i) == c_null_char) exit
            n = n + 1
        end do
        allocate (character(len=n) :: text)
        do i = 1, n
            text(i:i) = buffer(i)
        end do
    end function ptr_to_string

    ! Reads a heap string owned by the SQLite allocator and releases it.
    function take_string(ptr) result(text)
        type(c_ptr), intent(in) :: ptr
        character(len=:), allocatable :: text

        text = ptr_to_string(ptr, 65536)
        if (c_associated(ptr)) then
            call sqlite_core_string_free(ptr)
        end if
    end function take_string

    function free_string(ptr) result(text)
        type(c_ptr), intent(in) :: ptr
        character(len=:), allocatable :: text

        text = take_string(ptr)
    end function free_string

    ! static data (version, error_message, param_clean_prefix): copy it, never
    ! release the original.
    function static_string(ptr) result(text)
        type(c_ptr), intent(in) :: ptr
        character(len=:), allocatable :: text

        text = ptr_to_string(ptr, 65536)
    end function static_string

    ! The text of an err_buf filled by the library.
    function err_text(buf) result(text)
        character(kind=c_char), intent(in) :: buf(*)
        character(len=:), allocatable :: text
        integer :: i, n

        n = 0
        do i = 1, 512
            if (buf(i) == c_null_char) exit
            n = n + 1
        end do
        allocate (character(len=n) :: text)
        do i = 1, n
            text(i:i) = buf(i)
        end do
    end function err_text

    ! --- parameter builders ----------------------------------------------

    function param_value_int(value) result(out)
        integer(c_int64_t), intent(in) :: value
        type(param_value_t) :: out

        out%addr = transfer(value, c_null_ptr)
        out%len = 0
        out%max_len = 0
    end function param_value_int

    function param_value_double(value) result(out)
        real(c_double), intent(in) :: value
        type(param_value_t) :: out

        out%addr = transfer(value, c_null_ptr)
        out%len = 0
        out%max_len = 0
    end function param_value_double

    ! Text parameters own their native memory: the caller keeps the buffer
    ! alive for the whole call (see the example).
    function param_value_text(ptr) result(out)
        type(c_ptr), intent(in) :: ptr
        type(param_value_t) :: out

        ! len = -1 tells the library the pointer is NUL-terminated UTF-8.
        out%addr = ptr
        out%len = -1
        out%max_len = 0
    end function param_value_text

    ! Fills one parameter from caller-owned name/value buffers: the example
    ! keeps them alive for the whole call (c_loc of a local would dangle).
    subroutine set_param(p, name_buf, ftype, value)
        type(param_t), intent(out) :: p
        character(kind=c_char), target, intent(in) :: name_buf(*)
        integer(c_int), intent(in) :: ftype
        type(param_value_t), intent(in) :: value

        p%name = c_loc(name_buf)
        p%no_insensitive = 0
        p%ftype = ftype
        p%direction = direction_input
        p%value = value
    end subroutine set_param

    ! --- reader trampoline -----------------------------------------------
    ! Fortran builds the C callback with c_funloc of an interoperable
    ! procedure; the module-level counters keep the visited rows.

    function count_rows_cb(stmt, columns, hasrows, ctx) result(rc) bind(c)
        type(c_ptr), value :: stmt
        integer(c_int), value :: columns, hasrows
        type(c_ptr), value :: ctx
        integer(c_int) :: rc

        if (hasrows == 1) then
            cb_rows = cb_rows + 1
            cb_columns = columns
        end if
        rc = 0
    end function count_rows_cb

end module libsqlite
