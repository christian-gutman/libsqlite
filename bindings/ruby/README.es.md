# Ruby — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Ruby con la gema `ffi` (Fiddle de la stdlib queda descartado porque
`Fiddle::Closure` está roto en Ruby 3.x): los 34 símbolos congelados
enlazados uno a uno con su firma exacta, `ParamSet` que construye
`sqlite_core_param_t[]` en memoria cruda con desplazamientos válidos en LP64
e ILP32, `take_string` siguiendo la regla del alocador, buffers de salida
para `int*`, `long long` y `char**`, callback de filas con un `Proc` (ffi lo
convierte a la firma C), lectura de BLOB que crece el buffer, y el documento
JSON también construido con `sqlite_core_json_writer_callback`. Verificado en
el contenedor `ruby:3.3`.