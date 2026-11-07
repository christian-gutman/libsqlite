#ifndef SQLITE_CORE_BUILDER_H
#define SQLITE_CORE_BUILDER_H

#include "sqlite_core.h"

/**
 * @struct SqlLiteStringContext
 * @brief Estructura de control para la gestión de memoria dinámica en el generador de cadenas, JSON. XML, CSV
 * 
 * Esta estructura mantiene el estado persistente del búfer acumulador en el Heap de la RAM.
 * Viaja de forma opaca encapsulada a través del parámetro genérico 'pUserContext' exigido
 * por la firma de tu función callback corporativa 'SqlLiteCoreRowCallback'.
 */
typedef struct {
    char *string_ptr;         /**< Puntero base al bloque de caracteres del string JSON asignado en el Heap. */
    size_t allocated_size;  /**< Capacidad total de memoria (en bytes) asignada actualmente por malloc/realloc. */
    size_t current_len;     /**< Longitud real actual de la cadena acumulada (excluyendo el terminador nulo '\0'). */
    int rows_processed;     /**< Contador acumulativo del número de registros (filas) procesados con éxito. */
    int has_error;          /**< Bandera de estado crítico (Booleano). Se activa en 1 si ocurre un error Out-Of-Memory (OOM). */
} SqlLiteStringContext;


/**
 * @brief Callback especializado para la generación dinámica de flujos JSON.
 *        Cumple estrictamente con la firma SqlLiteCoreRowCallback de tu arquitectura Core.
 *
 * @param[in] stmt          Statement activo de SQLite en estado SQLITE_ROW.
 * @param[in] columns       Número neto de columnas devueltas por la consulta.
 * @param[in] hasrows       Bandera de control de presencia de filas.
 * @param[in] pUserContext  Puntero genérico que encapsula el contexto de memoria (SqlLiteJsonContext*).
 *
 * @return int              0 para continuar con la siguiente fila, 1 para abortar por error (OOM).
 */
CORE_API int SqlLite_Core_JsonWriterCallback(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);

    /**
 * @brief Procesa un arreglo JSON e inyecta dinámicamente sus registros en la tabla especificada.
 * 
 * Esta función realiza un análisis sintáctico lineal (parsing) en un solo barrido sobre una 
 * cadena JSON plana. Diseña de forma dinámica las instrucciones SQL estructuradas basándose 
 * en las propiedades del primer objeto detectado, eliminando la necesidad de esquemas cableados 
 * (hardcodeados) en tiempo de compilación.
 * 
 * @section seguridad Seguridad y Mitigación contra Inyecciones SQL
 * La función implementa de manera estricta el patrón de @b Sentencias @b Preparadas 
 * (Prepared Statements). En ningún escenario realiza concatenación de texto plano para inyectar 
 * los valores. En su lugar, utiliza comodines correlativos (`?`) y delega la asignación al motor 
 * binario mediante las funciones nativas de vinculación paramétrica (`sqlite3_bind_*`).
 * 
 * @section transacciones Rendimiento mediante Bloques Transaccionales
 * Con el fin de mitigar el impacto de E/S en el almacenamiento físico, el ciclo completo se encapsula 
 * de forma automática entre directivas `BEGIN TRANSACTION;` y `COMMIT;`. Esto agrupa múltiples 
 * inserciones en memoria RAM y realiza una única escritura atómica en el disco duro.
 * 
 * @param[in]  db           Puntero activo e inicializado a la conexión de la base de datos de SQLite. @b No @b puede @b ser @b NULL.
 * @param[in]  table_name   Cadena de caracteres (`C-String`) que especifica el nombre de la tabla destino en SQLite.
 * @param[in]  json_str     Cadena de caracteres que contiene el arreglo JSON estructurado (ej. `"[{"col": val}]"`).
 * @param[out] err_buf      Búfer de memoria asignado para almacenar el mensaje de error textual en caso de fallo. Puede ser `NULL`.
 * @param[in]  err_buf_max  Capacidad máxima en bytes reservada para @p err_buf para prevenir desbordamientos (*Buffer Overflow*).
 * 
 * @return int Cantidad de registros afectados o código de estado crítico:
 *         @retval >=0  Operación exitosa. Devuelve el número total de filas (objetos) insertadas en la base de datos.
 *         @retval SQLITE_MISUSE (21) Mal uso de la interfaz de SQLite o punteros críticos obligatorios pasados como @c NULL.
 *         @retval SQLITE_NOMEM (7) Fallo del sistema operativo al intentar redimensionar o asignar memoria en el Heap.
 *         @retval Otros Códigos Nativos Devuelve el código de error producido por el motor de SQLite (ej. fallos de constraints).
 * 
 * @note Si la inserción de una sola fila falla debido a violaciones de integridad (ej. una llave primaria duplicada 
 *       o una restricción @c NOT @c NULL), se ejecuta una llamada de contingencia a @c ROLLBACK; deshaciendo por 
 *       completo la tanda completa para evitar la corrupción o inconsistencia parcial de los datos.
 * 
 * @warning Los valores string extraídos del JSON se transfieren a SQLite utilizando la bandera @c SQLITE_TRANSIENT. 
 *          Esto fuerza a SQLite a realizar una copia interna de los datos, garantizando la seguridad de la memoria 
 *          pero incrementando marginalmente el consumo de RAM en payloads masivos.
 * 
 * @see SqlLite_Core_ExecuteReaderToJSON
 */

int SqlLite_Core_Insert_Json(
    sqlite3 *db,
    const char *table_name,
    const char *json_str,
    char *err_buf,
    int err_buf_max);    


#endif // SQLITE_CORE_BUILDER_H