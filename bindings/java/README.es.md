# Java — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Java completo con JNA: `LibSqlite.java` (los 34 símbolos congelados
+ las estructuras `sqlite_core_param_t` y el callback de filas) y
`ExampleFull.java` (el flujo canónico). Dependencia única: el jar de JNA,
que se descarga con `get_jna.ps1` y **no** se versiona. Trampas documentadas:
forzar `jna.encoding=UTF-8`, la lista `getFieldOrder()` terminante, los
pines de memoria nativa, y `toArray(n)` para los arrays de parámetros
contiguos. Verificado: 24 checks, 0 fallos (local x64 y contenedor Linux).