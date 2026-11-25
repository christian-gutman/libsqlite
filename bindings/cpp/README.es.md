# C++ — resumen en español

> Traducción del resumen de [`README.md`](README.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva la
> versión en español.

Binding C++ sin archivos propios que compilar: `libsqlite.hpp` declara los
34 símbolos congelados (`extern "C"`) y añade una capa fina —`ParamSet`
(dueño de la memoria nativa de nombre y valor, con el puntero capturado
antes de empujar el nombre), `Connection` (RAII), `take_string` /
`static_string` (regla del alocador), `get_blob` que crece el buffer,
`scalar_text` con su buffer de salida, y `static_assert` que detectan
cualquier deriva del layout de `sqlite_core_param_t`—. Verificado en el
contenedor Linux con `g++ -std=c++17 -Wall -Wextra`.