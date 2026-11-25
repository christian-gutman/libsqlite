dnl libsqlite extension — phpize build (Linux / macOS / any *nix + Windows with MSYS)
dnl
dnl The extension links against the libsqlite3 built by this repository
dnl (bin/<arch>-<os>/{shared,static}/<config>/libsqlite3.so) or a system one.
dnl Pass --with-libsqlite3-dir to point at the repository's own output.

PHP_ARG_ENABLE([libsqlite],
  [whether to enable libsqlite extension support],
  [AS_HELP_STRING([--enable-libsqlite],
    [Enable libsqlite extension support (links against libsqlite3)])],
  [no])

if test "$PHP_LIBSQLITE" != "no"; then
  PHP_LIBSQLITE_CFLAGS=""
  PHP_LIBSQLITE_LIBS="-lsqlite3"

  AC_MSG_CHECKING([for a libsqlite3 build directory])
  AC_ARG_WITH([libsqlite3-dir],
    [AS_HELP_STRING([--with-libsqlite3-dir=DIR],
      [Directory holding the libsqlite3 library and headers])],
    [
      PHP_LIBSQLITE_CFLAGS="-I$withval/include -I$withval"
      PHP_LIBSQLITE_LIBS="-L$withval -llibsqlite3 -Wl,-rpath,$withval"
    ],
    [AC_MSG_RESULT([no, using the system libsqlite3])])

  dnl -lsqlite3 maps to libsqlite3.so here, but the repository's artifact is
  dnl already named libsqlite3.so, so -llibsqlite3 would double the prefix on
  dnl some linkers; -l:libsqlite3.so names the file exactly.
  case "$PHP_LIBSQLITE_LIBS" in
    *-L*) : ;;
    *) PHP_LIBSQLITE_LIBS="-l:libsqlite3.so" ;;
  esac

  PHP_EVAL_INCLINE($PHP_LIBSQLITE_CFLAGS)
  PHP_EVAL_LIBLINE($PHP_LIBSQLITE_LIBS, LIBSQLITE_SHARED_LIBADD)

  dnl uintptr_t on macOS needs stdint through the extension's own includes.
  AC_CHECK_HEADERS([stdint.h], [], [AC_MSG_ERROR([stdint.h is required])])

  PHP_NEW_EXTENSION(libsqlite, libsqlite.c, $ext_shared)
  PHP_INSTALL_HEADERS([ext/libsqlite], [php_libsqlite.h])
fi