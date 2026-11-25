# Swift — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Swift: `include/libsqlite.h` + `module.modulemap` declaran la
superficie congelada (34 símbolos) para que `import libsqlite3` la importe
sin depender de las fuentes del repo, y `LibSqlite.swift` añade `Connection`
(clase que cierra en `deinit`), `ParamSet` (dueño de la memoria nativa de
nombre y valor) y `takeString`/`staticString` (regla del alocador). Verificado
en el contenedor `swift:6.0`.