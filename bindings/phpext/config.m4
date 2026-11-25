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
  dnl Two separate locations, because this repository separates them:
  dnl   --with-libsqlite3-src  directory holding sqlite_core.h and its
  dnl                          engine/ and compat/ siblings (src/sqlite/core)
  dnl   --with-libsqlite3-dir  directory holding the built library
  dnl                          (bin/<arch>-<os>/{shared,static}/<config>)
  dnl Assuming one place had both was the first mistake this extension made:
  dnl the container reported "sqlite_core.h: No such file or directory"
  dnl before any of the real code was even reached.
  PHP_LIBSQLITE_CFLAGS=""
  PHP_LIBSQLITE_LIBS=""

  AC_ARG_WITH([libsqlite3-src],
    [AS_HELP_STRING([--with-libsqlite3-src=DIR],
      [Directory containing sqlite_core.h (src/sqlite/core in this repo)])],
    [PHP_LIBSQLITE_CFLAGS="-I$withval"])

  AC_MSG_CHECKING([for the built libsqlite3 directory])
  AC_ARG_WITH([libsqlite3-dir],
    [AS_HELP_STRING([--with-libsqlite3-dir=DIR],
      [Directory containing the built libsqlite3 shared library])],
    [
      # Names the file exactly: -llibsqlite3 would double the "lib" prefix
      # against an artifact that is already called libsqlite3.so.
      PHP_LIBSQLITE_LIBS="-L$withval -l:libsqlite3.so -Wl,-rpath,$withval"
      AC_MSG_RESULT([$withval])
    ],
    [AC_MSG_RESULT([no])])

  if test -z "$PHP_LIBSQLITE_LIBS"; then
    AC_MSG_ERROR([--with-libsqlite3-dir is required: this extension has no
                  system libsqlite3 to fall back on, it links the one this
                  repository builds])
  fi

  AC_CHECK_HEADERS([stdint.h], [], [AC_MSG_ERROR([stdint.h is required])])

  PHP_EVAL_INCLINE($PHP_LIBSQLITE_CFLAGS)
  PHP_EVAL_LIBLINE($PHP_LIBSQLITE_LIBS, LIBSQLITE_SHARED_LIBADD)

  dnl uintptr_t on macOS needs stdint through the extension's own includes.

  PHP_NEW_EXTENSION(libsqlite, libsqlite.c, $ext_shared)
  PHP_INSTALL_HEADERS([ext/libsqlite], [php_libsqlite.h])
fi
