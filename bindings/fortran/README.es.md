# Fortran — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding Fortran puro con `iso_c_binding`: los 34 símbolos congelados como
bloques `interface` con `bind(c, name=...)` y tipos derivados `bind(C)` para
`sqlite_core_param_t` y su unión (un solo tipo `(addr, len, max_len)` donde
`dVal`/`iVal` comparten bytes con `blob_or_text.ptr`, escrito con
`transfer()`). La regla de oro: **el llamador es dueño de la memoria** —
`c_loc()` de un buffer local se cuelga al volver, así que todos los strings
que lee C (SQL, nombres, valores, buffer del scalar) se declaran en el
programa con `target`. El callback de filas se pasa con `c_funloc` y las
cadenas devueltas se liberan con `sqlite_core_string_free`. Verificado en el
contenedor (33 checks, 0 fallos).