# Julia — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Julia sin paquetes: `ccall` con la forma de solo nombre (la librería
se carga con `Libdl.dlopen(..., RTLD_GLOBAL)`), `ParamSet` que construye
`sqlite_core_param_t[]` en memoria cruda con desplazamientos derivados de
`Sys.WORD_SIZE ÷ 8`, `take_string`/`static_string` siguiendo la regla del
alocador, callback de filas con `@cfunction` (el estado vive en `Ref`s
globales porque los closures no sirven ahí) y el documento JSON también
construido con `sqlite_core_json_writer_callback`. Verificado en el
contenedor `julia:1.11`.