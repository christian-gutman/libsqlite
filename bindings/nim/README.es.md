# Nim — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Nim sin librería envolvente ni enlazado en tiempo de compilación:
los 34 símbolos congelados se declaran con `{.importc, dynlib: libname.}`
y se resuelven en tiempo de ejecución. La unión de `sqlite_core_param_t` se
modela con el pragma `{.union.}` (los 16 bytes de LP64 exactos),
`ParamSet` es dueño de las copias `cstring`, el callback de filas es un
`proc` de alto nivel `{.cdecl.}` (no captura entorno) y las cadenas
devueltas se liberan con `sqlite_core_string_free`. Verificado en el
contenedor `nimlang/nim:2.0.14`.