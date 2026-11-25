# Set-up de conexión: PRAGMAs, flags de compilación y qué hacer al abrir

> **Español.** Traducción de [`pragmas.md`](pragmas.md). La documentación
> canónica está en inglés (ver `CONTRIBUTING.md`); este fichero conserva
> la versión en español.

**English** · [Español](pragmas.md)

Cinco ajustes suelen proponerse juntos para una conexión SQLite "amigable
con la concurrencia". Esta página explica **qué hace cada uno**, **dónde
vive** (por conexión, en el archivo de base de datos o en la compilación) y
**cómo los aplica libsqlite**.

## Qué hace cada uno

| Ajuste | Efecto | Alcance |
|--------|--------|---------|
| `journal_mode = WAL` | Cambia el diario por un write-ahead log: lectores y escritor se bloquean entre sí (aparecen `-wal` y `-shm` junto al archivo). Requiere un archivo real (no `:memory:`) y permiso de escritura. | **Persistente en el archivo** |
| `synchronous = NORMAL` | Política de fsync. `FULL` sincroniza en cada commit; `NORMAL` solo en los checkpoints; `OFF` nunca. En WAL, `NORMAL` no pierde datos *confirmados* ante un fallo de aplicación; solo las últimas transacciones ante un corte de energía. En modo rollback, `NORMAL` puede corromper la base. | Por conexión (no persiste) |
| `busy_timeout = 5000` | Si otro escritor tiene el lock, la llamada espera hasta 5 s antes de fallar con `SQLITE_BUSY`. | Por conexión (no persiste) |
| `foreign_keys = ON` | Hace cumplir las claves foráneas declaradas (en SQLite vienen **apagadas por defecto en cualquier compilación**). | Por conexión (nunca persiste) |
| `temp_store = MEMORY` | Tablas e índices temporales en RAM en lugar de archivo temporal. | Por conexión (no persiste) |

**Juntos** son la configuración clásica de concurrencia multi-proceso con
durabilidad razonable: WAL permite lectores y un escritor simultáneos,
`busy_timeout` convierte la contención en espera en vez de error,
`foreign_keys` mantiene activa la integridad referencial, `temp_store` saca
el área temporal del disco, y `synchronous=NORMAL` es el nivel de fsync que
hace los commits WAL baratos sin perder datos confirmados ante fallos de
aplicación.

## Dónde se puede fijar cada uno

| Ajuste | Al abrir la conexión (código) | Valor por defecto de compilación (flag) |
|--------|-------------------------------|----------------------------------------|
| `journal_mode = WAL` | **Sí** — es una sentencia normal, y queda en el archivo (hasta `PRAGMA journal_mode = DELETE`) | **No hay flag**: el modo de diario es propiedad del *archivo*, no del build |
| `synchronous = NORMAL` | **Sí** | En parte: `-DSQLITE_DEFAULT_SYNCHRONOUS=1` (NORMAL) / `=2` (FULL), o `-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1` para bases WAL |
| `busy_timeout` | **Sí**, y ya está en la API: `sqlite_core_set_busy_timeout(db, 5000)` | **No hay flag**: siempre es por conexión |
| `foreign_keys = ON` | **Sí** | **Sí:** `-DSQLITE_DEFAULT_FOREIGN_KEYS=1` |
| `temp_store = MEMORY` | **Sí** | **Sí:** `-DSQLITE_TEMP_STORE=2` (memoria por defecto) / `=3` (memoria siempre) |

## Cómo los aplica libsqlite hoy

**Los cinco se pueden aplicar desde código al abrir la conexión, sin
tocar la librería** — `sqlite_core_execute_command` acepta SQL de varias
sentencias, así que el paquete es una sola llamada justo después de abrir
(las conexiones de solo lectura fallan en `journal_mode`, así que lo
omiten):

```c
sqlite3 *db = NULL;
int rc = 0;
db = sqlite_core_open_or_create("app.db", &rc);
if (db != NULL && rc == SQLITE_OK)
{
    sqlite_core_execute_command(db,
        "PRAGMA journal_mode = WAL;"
        "PRAGMA synchronous = NORMAL;"
        "PRAGMA foreign_keys = ON;"
        "PRAGMA temp_store = MEMORY;", NULL, 0);
    sqlite_core_set_busy_timeout(db, 5000);
}
```

Detalles que importan:

1. **Medido en esta librería** (abrir una base de archivo, aplicar el paquete
   con una sola llamada a `sqlite_core_execute_command` y releer los PRAGMAs):

   | PRAGMA | Valor por defecto (verificado) | Tras el paquete (verificado) |
   |--------|-------------------------------|------------------------------|
   | `journal_mode` | `delete` | `wal` — y sigue `wal` al reabrir el archivo |
   | `synchronous` | `2` (FULL) | `1` (NORMAL) |
   | `foreign_keys` | `0` (**APAGADO: el default de SQLite en todo build**) | `1` |
   | `temp_store` | `0` (archivo) | `2` (MEMORIA) |

   `sqlite_core_set_busy_timeout(db, 5000)` devuelve `1` (éxito) y un insert
   que viola una clave foránea declarada falla con
   `FOREIGN KEY constraint failed` — el ajuste se aplica, no solo se reporta.
   `foreign_keys` vuelve a `0` en una conexión nueva (confirma el alcance por
   conexión) y `journal_mode` vuelve a `wal` (confirma el persistente).
2. `busy_timeout` es la función de la API
   (`sqlite_core_set_busy_timeout`, ya cubierta por todos los bindings), así
   que se fija con la función en lugar del PRAGMA.
2. Cuatro de los cinco ajustes son **por conexión**: hay que aplicarlos
   después de *cada* apertura. Solo `journal_mode = WAL` persiste en el
   archivo — y falla en bases de solo lectura y en conexiones `:memory:`
   (que siempre usan modo `memory`).
3. `foreign_keys = ON` es el único con valor por defecto de compilación
   realmente útil, porque SQLite lo deja **apagado** en toda compilación si
   no se pide. Un build con `-DSQLITE_DEFAULT_FOREIGN_KEYS=1` elimina el
   "mis claves foráneas no hacen nada" más común.
4. `temp_store = MEMORY` y `synchronous = NORMAL` también tienen defaults
   compilables, pero cambiarlos en el build afecta a *todos* los
   consumidores de la librería; por eso la recomendación segura es el
   paquete por conexión documentado arriba, y los flags quedan para
   aplicaciones que controlan todo el build.
5. `synchronous = NORMAL` solo tiene sentido **junto con WAL**: en el modo
   rollback por defecto, `NORMAL` puede corromper la base ante un corte de
   energía — el par se fija (o ninguno).

## Opciones si se quiere dejar cocinado

Tres niveles, de menor a mayor invasividad (todos serían un cambio en la
librería, así que son propuestas, no algo hecho):

1. **Solo documentación (hecho arriba)** — cada consumidor aplica el paquete
   tras abrir; sin cambios binarios.
2. **Un ejemplo `LIBSQLITE3_RECOMMENDED_PRAGMAS`** en
   `src/sqlite/examples` (o un snippet en el README de cada binding) que
   abra, aplique el paquete y muestre el `PRAGMA journal_mode` /
   `foreign_keys` resultante — riesgo cero y visible en la suite CTest.
3. **Una opción de build** (p. ej. `-DLIBSQLITE3_ENABLE_WAL_DEFAULTS=ON`
   que añada `-DSQLITE_DEFAULT_FOREIGN_KEYS=1 -DSQLITE_TEMP_STORE=2
   -DSQLITE_DEFAULT_SYNCHRONOUS=1`) — solo para quien compila la librería;
   los flags quedan visibles en el cache de CMake y apagados por defecto,
   así que la API congelada de 34 símbolos y el comportamiento por defecto
   no cambian.
