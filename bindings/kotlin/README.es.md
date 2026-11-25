# Kotlin — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Kotlin con JNA (espejo del de Java): la interfaz `Core` declara los
34 símbolos congelados con su firma exacta y `libsqlite.kt` añade
`Sqlite.ParamSet` (que construye `sqlite_core_param_t[]` en memoria nativa y
conserva las copias de nombre/valor — igual que Ruby y Fortran, evitando
subclassificar `Structure`/`Union` de JNA) y `Sqlite.Connection` (handle +
todas las operaciones). El callback de filas es un `fun interface ...
Callback` (SAM) y las cadenas devueltas se liberan con
`sqlite_core_string_free`. Verificado en el contenedor
(`eclipse-temurin:17` + Kotlin + JNA).