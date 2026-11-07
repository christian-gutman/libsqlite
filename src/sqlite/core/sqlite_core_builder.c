#include "sqlite_core_builder.h"

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
int SqlLite_Core_JsonWriterCallback(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext)
{ 
    SqlLiteStringContext *ctx = (SqlLiteStringContext *)pUserContext;
    if (ctx == NULL || ctx->has_error)
    {
        return 1;  
    }
 
    if (hasrows == 0)
    {
        return 0;
    }
 
    char *string_ptr = ctx->string_ptr;
    size_t allocated_size = ctx->allocated_size;
    size_t current_len = ctx->current_len;
    int rows = ctx->rows_processed;
     /* Asegurar espacio seguro para la coma divisora ",\n" y la llave de apertura "  {\n" */
    if (current_len + 32 >= allocated_size)
    {
        allocated_size += 2048;
        char *temp_ptr = (char *)realloc(string_ptr, allocated_size);
        if (temp_ptr == NULL)
        {
            free(string_ptr);
            ctx->string_ptr = NULL;
            ctx->has_error = 1;
            return 1;
        }
        string_ptr = temp_ptr;
    } 
    if (rows > 0)
    {
        strcat(string_ptr, ",\n");
    }
    //
    strcat(string_ptr, "  {\n");
    current_len = strlen(string_ptr); 
    //
    for (int i = 0; i < columns; i++)
    {
        //
        const char *col_name = sqlite3_column_name(stmt, i);
        int col_type = sqlite3_column_type(stmt, i);
        // 
        size_t name_len = col_name ? strlen(col_name) : 0;
        size_t required_field_size = name_len + 64;
        //
        if (col_type != SQLITE_NULL)
        {
            required_field_size += (size_t)sqlite3_column_bytes(stmt, i);
        }
        //
        if (current_len + required_field_size >= allocated_size)
        {
            allocated_size += required_field_size + 4096; 
            char *temp_ptr = (char *)realloc(string_ptr, allocated_size);
            if (temp_ptr == NULL)
            {
                free(string_ptr);
                ctx->string_ptr = NULL;
                ctx->has_error = 1;
                return 1; 
            }
            string_ptr = temp_ptr;
        }
        //
        char *write_pos = string_ptr + current_len;
        //
        if (col_type == SQLITE_NULL)
        {
            snprintf(write_pos, required_field_size, "    \"%s\": null", col_name);
        }
        else if (col_type == SQLITE_INTEGER)
        {
            sqlite3_int64 val_int = sqlite3_column_int64(stmt, i);
            snprintf(write_pos, required_field_size, "    \"%s\": %lld", col_name, val_int);
        }
        else if (col_type == SQLITE_FLOAT)
        {
            double val_double = sqlite3_column_double(stmt, i);
            snprintf(write_pos, required_field_size, "    \"%s\": %.14g", col_name, val_double);
        }
        else
        {
            const char *val_utf8 = (const char *)sqlite3_column_text(stmt, i);
            const char *pFinalText = val_utf8;
            int must_free_ansi = 0;
            //
            if (val_utf8 != NULL)
            {
                //
                char *converted_ansi = SqlLite_Core_UTF8ToAnsi(val_utf8);
                if (converted_ansi != NULL)
                {
                    pFinalText = converted_ansi;
                    must_free_ansi = 1;
                }
            }
            //
            snprintf(write_pos, required_field_size, "    \"%s\": \"%s\"", col_name, pFinalText ? pFinalText : "");
            // 
            if (must_free_ansi)
            {
                SqlLite_Core_FreeString((void *)pFinalText);
            }
        }
        current_len = strlen(string_ptr);
        if (i < columns - 1)
        {
            strcat(string_ptr, ",\n");
        }
        else
        {
            strcat(string_ptr, "\n");
        }
        current_len = strlen(string_ptr);
    }
    //
    if (current_len + 8 >= allocated_size)
    {
        allocated_size += 512;
        char *temp_ptr = (char *)realloc(string_ptr, allocated_size);
        if (temp_ptr == NULL)
        {
            free(string_ptr);
            ctx->string_ptr = NULL;
            ctx->has_error = 1;
            return 1;
        }
        string_ptr = temp_ptr;
    }
    strcat(string_ptr, "  }");
    current_len = strlen(string_ptr);
    rows++;
    ctx->string_ptr = string_ptr;
    ctx->allocated_size = allocated_size;
    ctx->current_len = current_len;
    ctx->rows_processed = rows;
    return 0;
}

/* Helper básico en C ANSI para extraer el nombre de una propiedad
 * y su valor desde el objeto JSON plano */
static const char *parse_json_property(const char *json_pos, char *out_key, size_t key_max, char *out_val, size_t val_max, int *out_is_string, int *out_is_null)
{
    *out_is_string = 0;
    *out_is_null = 0;

    // Buscar el inicio de la llave '"'
    const char *k_start = strchr(json_pos, '"');
    if (!k_start || *k_start == '}' || *k_start == ']')
        return NULL;
    k_start++;

    const char *k_end = strchr(k_start, '"');
    if (!k_end)
        return NULL;

    size_t k_len = k_end - k_start;
    if (k_len >= key_max)
        k_len = key_max - 1;
    strncpy(out_key, k_start, k_len);
    out_key[k_len] = '\0';

    // Buscar los dos puntos ':'
    const char *colon = strchr(k_end, ':');
    if (!colon)
        return NULL;
    colon++;

    // Saltar espacios en blanco
    while (*colon == ' ' || *colon == '\t' || *colon == '\r' || *colon == '\n')
        colon++;

    // Determinar tipo de dato e interceptar valor
    if (*colon == '"')
    { // Es String
        *out_is_string = 1;
        colon++;
        const char *v_end = strchr(colon, '"');
        if (!v_end)
            return NULL;
        size_t v_len = v_end - colon;
        if (v_len >= val_max)
            v_len = val_max - 1;
        strncpy(out_val, colon, v_len);
        out_val[v_len] = '\0';
        return v_end + 1;
    }
    else
    { // Es Numérico o Null
        const char *v_end = colon;
        while (*v_end && *v_end != ',' && *v_end != '}' && *v_end != ' ' && *v_end != '\n' && *v_end != '\r')
            v_end++;
        size_t v_len = v_end - colon;
        if (v_len >= val_max)
            v_len = val_max - 1;
        strncpy(out_val, colon, v_len);
        out_val[v_len] = '\0';

        if (strcmp(out_val, "null") == 0)
        {
            *out_is_null = 1;
        }
        return v_end;
    }
}

/**
 * @file sqlite_core_json.c
 * @brief Componente de persistencia dinámica e inyección de datos JSON en SQLite.
 * @author Tu Nombre / Arquitectura de Software Core
 * @date 2026-06-27
 */

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
    int err_buf_max)
{
    if (db == NULL || table_name == NULL || json_str == NULL)
    {
        return SQLITE_MISUSE;
    }

    if (err_buf && err_buf_max > 0)
        err_buf[0] = '\0';

    // 1. Iniciar una transacción automática para optimizar drásticamente la velocidad de inserción masiva
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    int rows_inserted = 0;
    const char *token = strchr(json_str, '{');

    while (token != NULL)
    {
        // En el primer objeto deducimos las columnas para preparar la sentencia dinámicamente
        char sql_stmt[4096];
        char cols_part[2048] = "";
        char vals_part[2048] = "";

        char key[128];
        char val[1024];
        int is_string, is_null;

        // Primer barrido del objeto actual: Construir los nombres de columnas para el INSERT
        const char *scan = token;
        int col_idx = 0;
        while ((scan = parse_json_property(scan, key, sizeof(key), val, sizeof(val), &is_string, &is_null)) != NULL)
        {
            if (col_idx > 0)
            {
                strcat(cols_part, ", ");
                strcat(vals_part, ", ");
            }
            strcat(cols_part, key);
            // Usamos comodines correlativos "?", SQLite los mapea por índice basado en 1 (1, 2, 3...)
            strcat(vals_part, "?");
            col_idx++;
            if (*scan == '}')
                break;
        }

        if (col_idx == 0)
        {
            token = strchr(scan ? scan : token + 1, '{');
            continue;
        }

        // Armar la sentencia SQL final en memoria
        snprintf(sql_stmt, sizeof(sql_stmt), "INSERT INTO %s (%s) VALUES (%s);", table_name, cols_part, vals_part);

        // 2. Preparar el Statement binario en SQLite
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(db, sql_stmt, -1, &stmt, NULL);
        if (rc != SQLITE_OK)
        {
            if (err_buf && err_buf_max > 0)
            {
                snprintf(err_buf, err_buf_max, "Error al preparar INSERT: %s", sqlite3_errmsg(db));
            }
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return rc;
        }

        // Segundo barrido: Vincular (Bind) los valores reales al statement preparado de forma segura
        scan = token;
        int bind_idx = 1;
        while ((scan = parse_json_property(scan, key, sizeof(key), val, sizeof(val), &is_string, &is_null)) != NULL)
        {
            if (is_null)
            {
                sqlite3_bind_null(stmt, bind_idx);
            }
            else if (is_string)
            {
                // SQLITE_TRANSIENT le dice a SQLite que copie el string internamente
                // sqlite3_bind_text(stmt, bind_idx, val, -1, SQLITE_TRANSIENT);
                const char *pSrcText = val;
                int len_to_bind = -1; /* Al ser una variable local de C, medimos por terminación nula */
                int must_free = 0;
                if (pSrcText != NULL)
                {
                    /* Comprobación de codificación selectiva basada en tu arquitectura */
                    if (StrDetectEncoding(pSrcText) == ENCODING_ANSI)
                    {
                        char *converted_text = SqlLite_Core_AnsiToUTF8(pSrcText);
                        if (converted_text != NULL)
                        {
                            pSrcText = converted_text;
                            len_to_bind = -1;
                            must_free = 1;
                        }
                    }
                }

                sqlite3_bind_text(stmt, bind_idx, pSrcText, len_to_bind, SQLITE_TRANSIENT);

                if (must_free)
                {
                    SqlLite_Core_FreeString((void *)pSrcText);
                }
            }
            else
            {
                // Validar si es punto flotante o entero para hacer el bind numérico correcto
                if (strchr(val, '.') != NULL)
                {
                    double val_double = atof(val);
                    sqlite3_bind_double(stmt, bind_idx, val_double);
                }
                else
                {
                    sqlite3_int64 val_int = _strtoui64(val, NULL, 10); // o atoll(val) en C99
                    sqlite3_bind_int64(stmt, bind_idx, val_int);
                }
            }
            bind_idx++;
            if (*scan == '}')
                break;
        }

        // 3. Ejecutar la inserción física
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt); // Liberar el statement de inmediato

        if (rc != SQLITE_DONE)
        {
            if (err_buf && err_buf_max > 0)
            {
                snprintf(err_buf, err_buf_max, "Error de ejecucion en fila %d: %s", rows_inserted + 1, sqlite3_errmsg(db));
            }
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return rc;
        }

        rows_inserted++;
        token = strchr(scan, '{'); // Buscar el siguiente objeto JSON dentro del arreglo
    }

    // 4. Confirmar los cambios en el archivo físico de base de datos
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return rows_inserted; // Retorna cuántos registros se insertaron con éxito
}
