# R — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding R en dos capas: `libsqlite.R` (wrappers R + la tabla `SYMBOLS` con
los 34 símbolos congelados) y `src/libsqlite_shim.c` (compilado con
`R CMD SHLIB`), necesario porque `.C()` descarta el valor de retorno (el
handle `sqlite3*` sería inalcanzable), no puede construir
`sqlite_core_param_t` (unión con punteros nativos), no representa
`sqlite3_int64` ni puede convertir un closure de R en el callback de C que
pide `execute_reader`. El shim no reimplementa nada: llama a la API
congelada, lee y libera sus cadenas con `sqlite_core_string_free`, y hasta
ejercita `sqlite_core_json_writer_callback`. Verificado en el contenedor
Linux: 25 checks, 0 fallos.