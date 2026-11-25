#ifndef PHP_LIBSQLITE_H
#define PHP_LIBSQLITE_H

#define PHP_LIBSQLITE_VERSION "1.0.0"
#define PHP_LIBSQLITE_EXTNAME "libsqlite"

extern zend_module_entry libsqlite_module_entry;
#define phpext_module_ptr &libsqlite_module_entry

/* Exposed to libsqlite.c only; PHP_MINIT/… live in libsqlite.c. */
PHP_MINIT_FUNCTION(libsqlite);
PHP_MSHUTDOWN_FUNCTION(libsqlite);
PHP_RINIT_FUNCTION(libsqlite);
PHP_RSHUTDOWN_FUNCTION(libsqlite);
PHP_MINFO_FUNCTION(libsqlite);

#endif /* PHP_LIBSQLITE_H */