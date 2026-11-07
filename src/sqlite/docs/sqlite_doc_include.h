#ifndef SQLITE_EXPORT_H
#define SQLITE_EXPORT_H

/**
 * @file sqlite_core_types.h
 * @brief Definiciones de tipos, macros de exportación y estructuras fundamentales del núcleo SQLite.
 *
 * Este archivo centraliza la configuración de compilación multiplataforma, las directivas de visibilidad
 * de símbolos para la generación de DLL/SO, y la abstracción del mapeo de tipos y parámetros necesarios
 * para la interoperabilidad segura con lenguajes de escritorio (como Microsoft Visual FoxPro).
 */

// ============================================================================
// CONFIGURACIÓN DE OPTIMIZACIÓN DE SQLITE (NÚCLEO)
// ============================================================================
// #define SQLITE_CORE 1
// #define SQLITE_THREADSAFE 1
// #define SQLITE_OMIT_LOAD_EXTENSION 1
// #define SQLITE_OMIT_PROGRESS_CALLBACK 1
// #define SQLITE_OMIT_SHARED_CACHE 1
// #define SQLITE_OMIT_UTF16 1
// #define SQLITE_OMIT_DEPRECATED 1
// #define SQLITE_DEFAULT_FOREIGN_KEYS 1

#include "engine/sqlite3.h"

// Evita conflictos de re-declaración de tipos opacos de SQLite
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

/**
 * @def SQLITE_CORE_API
 * @brief Macro para la visibilidad y exportación de símbolos en bibliotecas dinámicas (DLL / SO).
 *
 * En sistemas Windows (`_WIN32`) expone los símbolos mediante `__declspec(dllexport)`.
 * En sistemas POSIX con GCC versión 4 o superior, inyecta el atributo `visibility("default")`
 * para garantizar la correcta exposición de la interfaz binaria (ABI).
 */
#ifndef SQLITE_CORE_API
#if defined(_WIN32) || defined(SQLITE_CORE_BUILD_DLL) || defined(__CYGWIN__)
#define SQLITE_CORE_API __declspec(dllexport)
#else
#if __GNUC__ >= 4
#define SQLITE_CORE_API __attribute__((visibility("default")))
#else
#define SQLITE_CORE_API
#endif
#endif
#endif

/**
 * @def SQLITE_CALL
 * @brief Define la convención de llamadas para las funciones exportadas.
 *
 * Si el firmware se compila bajo el ecosistema de Visual FoxPro (`VFP_FLL`), se configura la convención
 * `@b __fastcall` (paso de argumentos por registros). Para cualquier otro entorno estándar, se
 * fuerza el uso seguro de `@b __stdcall` para asegurar la estabilidad de la pila (Stack).
 */
#if defined(VFP_FLL)
#define SQLITE_CALL __fastcall
#else
#define SQLITE_CALL __stdcall
#endif

/**
 * @def SQL_LITE_EXPORT
 * @brief Decorador unificado que combina la directiva de visibilidad de la API y su convención de llamadas.
 *
 * Todas las funciones públicas expuestas en la frontera de la DLL/SO deben ser declaradas utilizando este prefijo.
 */
#define SQL_LITE_EXPORT SQLITE_CORE_API SQLITE_CALL

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <malloc.h>
/**
 * @brief Macro de abstracción para la comparación completa de cadenas (Windows).
 */
#define str_compare _stricmp
/**
 * @brief Macro de abstracción para la comparación limitada por longitud de cadenas (Windows).
 */
#define str_n_compare _strnicmp
#else
#include <strings.h>
#include <unistd.h>
/**
 * @brief Macro de abstracción para la comparación completa de cadenas (POSIX).
 */
#define str_compare strcasecmp
/**
 * @brief Macro de abstracción para la comparación limitada por longitud de cadenas (POSIX).
 */
#define str_n_compare strncasecmp
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>

/**
 * @name Constantes de Codificación de Texto
 * @{
 */
#define ENCODING_ASCII 1
#define ENCODING_UTF8 2
#define ENCODING_ANSI 3 // Windows-1252 / FoxPro por defecto

/**
 * @brief Tipos de datos nativos soportados por el motor de abstracción.
 */
typedef enum
{
    SQLITE_CORE_TYPE_NULL = 0,
    SQLITE_CORE_TYPE_INT = 2,
    SQLITE_CORE_TYPE_DOUBLE = 4,
    SQLITE_CORE_TYPE_TEXT = 6,
    SQLITE_CORE_TYPE_BLOB = 8
} SqlLiteCoreParamType;

/**
 * @brief Dirección lógica del parámetro para el control del flujo y mapeo de datos.
 */
typedef enum
{
    SQLITE_CORE_DIRECTION_INPUT = 1,
    SQLITE_CORE_DIRECTION_OUTPUT = 2,
    SQLITE_CORE_DIRECTION_INPUTOUTPUT = 3
} SqlLiteCoreParameterDirection;

/**
 * @brief Estructura unificada para la definición, vinculación y extracción de parámetros SQL.
 *
 * Actúa como la entidad central para el intercambio seguro de datos entre la capa lógica de la aplicación
 * y las funciones de inyección y extracción de SQLite. Utiliza una unión interna para optimizar el espacio
 * físico ocupado en la memoria RAM.
 */
typedef struct
{
    const char *name;                        /**< Nombre del parámetro asignado en la sentencia SQL (ej: ":codigo", "@id"). */
    int no_insensitive;                      /**< Bandera de control: `1` fuerza comparación Case-Sensitive estricta; `0` permite Case-Insensitive flexible. */
    SqlLiteCoreParamType type;               /**< Discriminador del tipo de dato contenido en el parámetro. */
    SqlLiteCoreParameterDirection Direction; /**< Dirección de flujo configurada para el ciclo de vida del parámetro. */
    union
    {
        double dVal; /**< Almacenamiento directo para valores de tipo @ref SQLITE_CORE_TYPE_DOUBLE. */
        int iVal;    /**< Almacenamiento directo para valores de tipo @ref SQLITE_CORE_TYPE_INT. */
        struct
        {
            char *ptr;   /**< Puntero de memoria que aloja el buffer de texto o la secuencia binaria del BLOB. */
            int len;     /**< Longitud lineal actual del flujo de datos medido en bytes. */
            int max_len; /**< Capacidad física máxima asignada al buffer @p ptr (Crítico para evitar desbordamientos en OUTPUT). */
        } blob_or_text;  /**< Estructura de control especializada para tipos de datos dinámicos (@ref SQLITE_CORE_TYPE_TEXT y @ref SQLITE_CORE_TYPE_BLOB). */
    } value;             /**< Unión contenedora del valor nativo discriminado por el tipo de parámetro. */
} SqlLiteCoreParam;

/**
 * @brief Definición de tipo (Typedef) para la función de callback encargada de procesar filas de resultados.
 *
 * Define la firma del puntero de función que se invocará de forma iterativa por cada registro o fila
 * recuperada exitosamente durante la fase de evaluación de una consulta SQL (`sqlite3_step`).
 *
 * @param[in] stmt          Puntero a la sentencia activa de SQLite (`sqlite3_stmt`) posicionada en la fila actual.
 * @param[in] columns       Cantidad total de columnas físicas devueltas por el conjunto de resultados.
 * @param[in] hasrows       Bandera booleana rápida: `1` indica la presencia de datos válidos; `0` indica conjunto vacío.
 * @param[in] pUserContext  Puntero opcional de propósito general para inyectar el estado o contexto del llamador (State Pattern).
 *
 * @return int Estado de control del procesamiento. Un retorno de `0` aborta inmediatamente la lectura del cursor.
 */
typedef int(SQLITE_CALL *SqlLiteCoreRowCallback)(
    sqlite3_stmt *stmt,
    int columns,
    int hasrows,
    void *pUserContext);

#ifdef __cplusplus
extern "C"
{
#endif

    // ============================================================================
    // STRINGS - AUTOCOMPLETADO
    // ============================================================================

    /**
     * @brief Rellena una cadena de texto por la derecha hasta alcanzar una longitud total específica.
     *
     * Copia la cadena de origen al inicio del buffer de destino y completa el espacio restante escribiendo
     * el carácter de relleno (`pad_char`) al final. Si la longitud de la cadena fuente (`src`) supera o iguala
     * el tamaño total solicitado, se aplica un truncamiento automático llamando a `SqlLite_Core_StrLeft`.
     *
     * @param[out] dest      Puntero al buffer de destino preasignado en memoria.
     * @param[in]  src       Puntero a la cadena de caracteres de origen (lectura).
     * @param[in]  total_len Longitud final deseada para la cadena resultante (sin contar el terminador nulo).
     * @param[in]  pad_char  Carácter que se utilizará para realizar el relleno (ej: '0' o ' ').
     *
     * @return char* Puntero al buffer @p dest que contiene la cadena formateada con relleno y terminada en cero.
     * @retval dest  Retorna el mismo puntero original de destino si @p dest o @p src son nulos.
     *
     * @warning **Seguridad de Memoria:** Al igual que `PadL`, el buffer @p dest debe contar con un espacio
     *          de almacenamiento asignado de forma previa de al menos `total_len + 1` bytes.
     *
     * @see Str_Core_PadL
     * @see SqlLite_Core_StrLeft
     */
    char *Str_Core_PadL(char *dest, const char *src, size_t total_len, char pad_char);

    /**
     * @brief Rellena una cadena de texto por la izquierda hasta alcanzar una longitud total específica.
     *
     * Añade caracteres de relleno (`pad_char`) al inicio del buffer antes de copiar la cadena de origen.
     * Si la longitud de la cadena fuente (`src`) ya es igual o mayor al tamaño total solicitado, la función
     * trunca el texto de forma automática desde el extremo izquierdo llamando a `SqlLite_Core_StrLeft`.
     *
     * @param[out] dest      Puntero al buffer de destino preasignado en memoria.
     * @param[in]  src       Puntero a la cadena de caracteres de origen (lectura).
     * @param[in]  total_len Longitud final deseada para la cadena resultante (sin contar el terminador nulo).
     * @param[in]  pad_char  Carácter que se utilizará para realizar el relleno (ej: '0' o ' ').
     *
     * @return char* Puntero al buffer @p dest que contiene la cadena formateada con relleno y terminada en cero.
     * @retval dest  Retorna el mismo puntero original de destino si @p dest o @p src son nulos.
     *
     * @warning **Seguridad de Memoria:** El llamador debe garantizar rigurosamente que el buffer @p dest
     *          tenga una capacidad física asignada de al menos `total_len + 1` bytes para evitar desbordamientos.
     *
     * @see Str_Core_PadR
     * @see SqlLite_Core_StrLeft
     */
    char *Str_Core_PadR(char *dest, const char *src, size_t total_len, char pad_char);

    // ============================================================================
    // STRINGS - NORMALIZACION
    // ============================================================================

    /**
     * @brief Normaliza una cadena de texto aplicando una limpieza de espacios y convirtiéndola a minúsculas.
     *
     * Esta función consolida tres operaciones lógicas de formateo en un único recorrido eficiente:
     * 1. Remueve todos los espacios en blanco iniciales (prefijo) avanzando el puntero de lectura.
     * 2. Descarta todos los espacios en blanco finales (sufijo) acotando la longitud de copiado.
     * 3. Transforma cada carácter útil remanente a minúsculas mediante `tolower()`.
     *
     * @param[out] dest Puntero al buffer de destino preasignado donde se escribirá la cadena normalizada.
     * @param[in]  src  Puntero a la cadena de caracteres original de origen (lectura).
     *
     * @note **Seguridad contra Caracteres Extendidos:** Aplica de forma sistemática un moldeo a `unsigned char`
     *       en las macros `isspace()` y `tolower()` para neutralizar comportamientos indefinidos con caracteres
     *       acentuados o de signo extendido.
     * @warning **Seguridad de Memoria:** El buffer de destino @p dest debe estar previamente alojado por el
     *          llamador y contar con una capacidad de almacenamiento de al menos `strlen(src) + 1` bytes.
     *
     * @see Str_Core_TrimAll
     * @see Str_Core_ToLower
     */
    void Str_Core_Normalize(char *dest, const char *src);

    // ============================================================================
    // STRINGS - REMOVER U EXTRAER CARACTERES
    // ============================================================================

    /**
     * @brief Extrae de forma ultra segura los primeros 'N' caracteres de una cadena, validando los límites del buffer destino.
     *
     * Copia un máximo de @p n caracteres desde la cadena de origen hacia el buffer de destino. A diferencia de
     * `SqlLite_Core_StrLeft`, esta función exige el tamaño físico real del buffer receptor (`dest_size`) para truncar
     * matemáticamente la operación y garantizar que jamás ocurra un desbordamiento de memoria, reservando siempre
     * el último byte para el terminador nulo.
     *
     * @param[out] dest      Puntero al buffer de destino preasignado en memoria.
     * @param[in]  dest_size Capacidad total física en bytes asignada al buffer @p dest.
     * @param[in]  src       Puntero a la cadena de caracteres de origen (lectura).
     * @param[in]  n         Cantidad deseada de caracteres a extraer desde la izquierda.
     *
     * @return char* Puntero al buffer @p dest que contiene la subcadena extraída y terminada de forma segura en cero.
     * @retval dest  Retorna el mismo puntero original de destino si @p dest o @p src son nulos, o si @p dest_size es cero.
     *
     * @see Str_Core_Left
     * @see Str_Core_Right
     */
    char *Str_Core_LeftSafe(char *dest, size_t dest_size, const char *src, size_t n);

    /**
     * @brief Extrae los primeros 'N' caracteres de una cadena desde el extremo izquierdo.
     *
     * Realiza una copia lineal de @p n bytes desde el inicio de la cadena de origen mediante `memcpy()`.
     * Cuenta con validación interna para no leer más allá de la longitud real de la cadena fuente si @p n es excesivo.
     *
     * @param[out] dest Puntero al buffer de destino preasignado en memoria.
     * @param[in]  src  Puntero a la cadena de caracteres de origen (lectura).
     * @param[in]  n    Cantidad de caracteres a extraer desde la izquierda.
     *
     * @return char* Puntero al buffer @p dest con la subcadena resultante y terminada en cero.
     * @retval dest  Retorna el mismo puntero original de destino si @p dest o @p src son nulos.
     *
     * @warning **Riesgo de Desbordamiento:** Esta función asume que el buffer @p dest tiene espacio suficiente
     *          para almacenar al menos `n + 1` bytes. Si se desconoce el tamaño real de destino, se recomienda
     *          utilizar rigurosamente @ref SqlLite_Core_StrLeftSafe.
     *
     * @see Str_Core_LeftSafe
     * @see Str_Core_Right
     */
    char *Str_Core_Left(char *dest, const char *src, size_t n);

    /**
     * @brief Extrae los últimos 'N' caracteres de una cadena desde el extremo derecho.
     *
     * Calcula la posición de inicio mediante aritmética de punteros avanzándose a `src + (len - n)`, y copia
     * los últimos @p n caracteres residuales de la cadena origen hacia el buffer destino.
     *
     * @param[out] dest Puntero al buffer de destino preasignado en memoria.
     * @param[in]  src  Puntero a la cadena de caracteres de origen (lectura).
     * @param[in]  n    Cantidad de caracteres a extraer desde la derecha.
     *
     * @return char* Puntero al buffer @p dest con la subcadena de cierre resultante y terminada en cero.
     * @retval dest  Retorna el mismo puntero original de destino si @p dest o @p src son nulos.
     *
     * @warning **Riesgo de Desbordamiento:** Al igual que `StrLeft`, carece de validación sobre el tamaño del
     *          buffer receptor. El llamador debe asegurar que @p dest cuente con espacio para un mínimo de `n + 1` bytes.
     *
     * @see Str_Core_LeftSafe
     * @see Str_Core_Right
     */
    char *Str_Core_Right(char *dest, const char *src, size_t n);

    /**
     * @brief Remueve de forma segura todos los espacios en blanco al inicio y al final de una cadena de texto.
     *
     * Esta función limpia los caracteres invisibles o de espaciado (tales como espacios, tabulaciones `\t`,
     * saltos de línea `\n` y retornos de carro `\r`) evaluándolos mediante `isspace()`. Utiliza aritmética
     * de punteros para saltar el prefijo y decrementa la longitud para descartar el sufijo, realizando
     * finalmente una copia optimizada mediante `memcpy()`.
     *
     * @param[out] dest Puntero al buffer de destino preasignado donde se guardará la cadena limpia.
     * @param[in]  src  Puntero a la cadena de caracteres original de origen (lectura).
     *
     * @return char* Puntero al buffer @p dest que contiene el texto formateado y terminado en cero.
     * @retval dest  Retorna el mismo puntero de destino original si @p dest o @p src son nulos.
     *
     * @warning **Seguridad de Memoria:** El buffer de destino @p dest debe estar preasignado en memoria por el
     *          llamador y contar con una capacidad mínima de `strlen(src) + 1` bytes para mitigar cualquier
     *          riesgo de desbordamiento de buffer (`Buffer Overflow`).
     *
     * @note Se aplica un moldeo preventivo a `unsigned char` en cada evaluación de `isspace()` para garantizar
     *       la estabilidad del código frente a tablas de caracteres extendidas o caracteres con signo.
     */
    char *Str_Core_TrimAll(char *dest, const char *src);

    // ============================================================================
    // STRINGS - CAPITALIZACION
    // ============================================================================

    /**
     * @brief Convierte todos los caracteres de una cadena de texto a minúsculas.
     *
     * Recorre secuencialmente la cadena de origen byte por byte y realiza la conversión utilizando
     * la función estándar `tolower()`. Aplica de forma segura un moldeo a `unsigned char` para evitar
     * comportamientos indefinidos con caracteres de signo extendido (valores mayores a 127).
     *
     * @param[out] dest Puntero al buffer de destino preasignado donde se escribirá el resultado.
     * @param[in]  src  Puntero a la cadena de caracteres de origen (lectura).
     *
     * @return char* Puntero al buffer @p dest que contiene la cadena convertida y terminada en cero.
     * @retval dest  Retorna el mismo puntero de destino original de forma segura si @p dest o @p src son nulos.
     *
     * @warning **Seguridad de Memoria:** El buffer de destino @p dest debe estar preasignado en memoria por el
     *          llamador y contar con una capacidad mínima de `strlen(src) + 1` bytes para evitar un desbordamiento.
     *
     * @see Str_Core_ToUpper
     */
    char *Str_Core_ToLower(char *dest, const char *src);

    /**
     * @brief Convierte todos los caracteres de una cadena de texto a mayúsculas.
     *
     * Recorre secuencialmente la cadena de origen byte por byte y realiza la conversión utilizando
     * la función estándar `toupper()`. Aplica un moldeo preventivo a `unsigned char` para procesar
     * de manera estable caracteres con signo en tablas de códigos extendidas.
     *
     * @param[out] dest Puntero al buffer de destino preasignado donde se escribirá el resultado.
     * @param[in]  src  Puntero a la cadena de caracteres de origen (lectura).
     *
     * @return char* Puntero al buffer @p dest que contiene la cadena convertida y terminada en cero.
     * @retval dest  Retorna el mismo puntero de destino original de forma segura si @p dest o @p src son nulos.
     *
     * @warning **Seguridad de Memoria:** Al igual que su contraparte, el llamador debe asegurar que @p dest
     *          tenga un tamaño de almacenamiento asignado de al menos `strlen(src) + 1` bytes.
     *
     * @see Str_Core_ToLower
     */
    char *Str_Core_ToUpper(char *dest, const char *src);

    // ============================================================================
    // STRINGS - FORMATEO - CONCATENACION
    // ============================================================================

    /**
     * @brief Da formato a una cadena de texto basada en una plantilla estándar con argumentos variables y asignación dinámica de memoria.
     *
     * Esta función calcula en un primer paso el tamaño exacto requerido para la cadena formateada mediante `vsnprintf`.
     * Posteriormente, realiza una asignación dinámica de memoria en el Heap del sistema y ejecuta un segundo paso
     * de escritura utilizando una copia segura de la lista de argumentos variables (`va_copy`). Previene de forma
     * nativa errores comunes de desbordamiento de buffer (`Buffer Overflow`).
     *
     * @param[in] formato Cadena de plantilla que define las especificaciones de formato estándar de la biblioteca de C.
     * @param[in] ...     Lista de argumentos variables correspondientes a los tokens de la plantilla.
     *
     * @return char* Puntero a la nueva cadena de texto formateada y terminada en cero alojada en memoria.
     * @retval NULL  Si el parámetro @p formato es nulo, si ocurrió un error en el cálculo de la longitud,
     *               o si falló la asignación de memoria en el Heap (`malloc`).
     *
     * @note **Gestión de Memoria:** A diferencia de las funciones basadas en el motor de SQLite, esta rutina utiliza
     *       el asignador nativo del sistema (`malloc()`). Por lo tanto, el llamador **debe** liberar de forma obligatoria
     *       este recurso invocando la función estándar `free()` cuando deje de utilizarlo.
     * @note **Portabilidad de Argumentos:** El uso de `va_copy` garantiza la estabilidad del flujo y la portabilidad
     *       del código en arquitecturas donde las listas de argumentos variables (`va_list`) se pasan por referencia
     *       o se modifican de forma destructiva en el primer recorrido.
     *
     * @see SqlLite_Core_Format
     */
    char *Str_Concat_Format(const char *formato, ...);

    // ============================================================================
    // STRINGS - COMPARACION DE CADENAS
    // ============================================================================

    /**
     * @brief Compara dos cadenas de texto completas sin distinción de mayúsculas (Case-Insensitive).
     *
     * Encapsula de manera transparente las funciones del sistema nativo (`_stricmp` en Windows
     * o `strcasecmp` en sistemas POSIX) para realizar una evaluación léxica de caracteres.
     *
     * @param[in] str1 Puntero a la primera cadena de texto que se va a comparar.
     * @param[in] str2 Puntero a la segunda cadena de texto que se va a comparar.
     *
     * @return int Resultado de la evaluación léxica.
     * @retval 0  Si el contenido de ambas cadenas es exactamente idéntico (ignorando mayúsculas/minúsculas).
     * @retval <0 Si el primer carácter que no coincide tiene un valor menor en @p str1 que en @p str2.
     * @retval >0 Si el primer carácter que no coincide tiene un valor mayor en @p str1 que en @p str2.
     *
     * @see Str_Core_NCompare
     */
    int Str_Core_Compare(const char *str1, const char *str2);

    /**
     * @brief Compara los primeros 'N' caracteres de dos cadenas sin distinción de mayúsculas (Case-Insensitive).
     *
     * Realiza una comparación léxica acotada, deteniéndose inmediatamente al alcanzar el límite de bytes
     * especificado por el parámetro @p _MaxCount o al encontrar un terminador nulo (`\0`).
     *
     * @param[in] str1      Puntero a la primera cadena de texto.
     * @param[in] str2      Puntero a la segunda cadena de texto.
     * @param[in] _MaxCount Cantidad máxima de caracteres (bytes) secuenciales que se van a evaluar.
     *
     * @return int Resultado de la evaluación léxica acotada.
     * @retval 0  Si los primeros @p _MaxCount caracteres de ambas cadenas coinciden plenamente.
     * @retval <0 Si la subcadena evaluada de @p str1 es menor léxicamente que la de @p str2.
     * @retval >0 Si la subcadena evaluada de @p str1 es mayor léxicamente que la de @p str2.
     *
     * @see Str_Core_Compare
     */
    int Str_Core_NCompare(const char *str1, const char *str2, size_t _MaxCount);

    // ============================================================================
    // ESCAPE / UNESCAPE DE CADENAS
    // ============================================================================

    /**
     * @brief Escapa comillas simples en una cadena de texto duplicándolas para sentencias SQL literales.
     *
     * Recorre la cadena de entrada y copia cada carácter en el buffer de salida provisto por el usuario.
     * Si detecta una comilla simple (`'`), inserta una comilla simple adicional (`''`) para cumplir con
     * las reglas de escape de cadenas literales de SQLite.
     *
     * @param[in]  input  Puntero a la cadena de caracteres original de origen.
     * @param[in]  len    Cantidad de caracteres a leer de la cadena @p input.
     * @param[out] output Puntero al buffer de destino preasignado donde se escribirá el resultado.
     *
     * @return char* Puntero al buffer @p output que contiene la cadena escapada y terminada en cero.
     * @retval NULL  Si @p input o @p output son nulos, o si @p len es menor o igual a cero.
     *
     * @warning **Seguridad de Memoria:** El buffer @p output debe tener suficiente espacio asignado de forma
     *          previa por el llamador. En el peor escenario posible (donde la cadena de entrada consista
     *          únicamente de comillas simples), el tamaño requerido será de `(len * 2) + 1` bytes.
     *
     * @see Str_Core_Unescape
     */
    char *Str_Core_Escape(const char *input, int len, char *output);

    /**
     * @brief Revierte el escape de comillas simples, transformando secuencias de doble comilla en una sola.
     *
     * Analiza una cadena previamente escapada y procesa sus caracteres. Cuando encuentra una secuencia
     * consecutiva de dos comillas simples (`''`), la unifica reduciéndola a una sola comilla en el buffer de salida.
     *
     * @param[in] input Puntero a la cadena de caracteres con el texto escapado.
     * @param[in] len   Cantidad de caracteres a leer de la cadena @p input.
     *
     * @return char* Puntero a la nueva cadena de texto desescapada y terminada en cero alojada en memoria.
     * @retval NULL  Si @p input es nulo, si @p len es inválido, o si falló la asignación dinámica de memoria.
     *
     * @note **Gestión de Memoria:** Esta función reserva memoria de forma dinámica utilizando `malloc()`. El llamador
     *       tiene la responsabilidad absoluta de liberar este buffer mediante un bloque `free()` estándar del sistema
     *       cuando deje de utilizarlo.
     *
     * @see SqlLite_Core_Escape
     */
    char *Str_Core_Unescape(const char *input, int len);

    /**
     * @brief Concatena dos cadenas de texto de forma segura, calculando y asignando la memoria exacta requerida.
     *
     * Une linealmente el contenido de dos flujos de texto en un nuevo buffer dinámico del Heap. Cuenta con un
     * mecanismo de tolerancia a fallos que intercepta de manera transparente los parámetros nulos para prevenir
     * accesos no válidos a la memoria física de la aplicación.
     *
     * @param[in] str1 Puntero a la primera cadena de texto (prefijo).
     * @param[in] str2 Puntero a la segunda cadena de texto (sufijo).
     *
     * @return char* Puntero al nuevo buffer dinámico que contiene la unión de ambas cadenas terminada en cero.
     * @retval NULL  Si ambos parámetros (@p str1 y @p str2) son nulos simultáneamente.
     *
     * @note **Comportamiento en Punteros Nulos:** Si únicamente un parámetro es nulo, la función genera y
     *       devuelve una copia duplicada e independiente del parámetro válido restante utilizando `sqlite3_mprintf`.
     * @note **Gestión de Memoria:** Al igual que el formateador del núcleo, la cadena devuelta reside en el Heap
     *       del motor y requiere ser destruida por el llamador mediante la función @ref SqlLite_Core_FreeString.
     *
     * @see SqlLite_Core_FreeString
     * @see SqlLite_Core_Format
     */
    char *SqlLite_Core_Concat(const char *str1, const char *str2);

    // ============================================================================
    // TRATAMIENTO DE CADENAS
    // ============================================================================

    /**
     * @brief Da formato a una cadena de texto basada en una plantilla con argumentos variables y asignación automática de memoria.
     *
     * Utiliza internamente el formateador seguro de SQLite (`sqlite3_vmprintf`) para procesar una lista de
     * argumentos variables (`va_list`). Mide de forma transparente el tamaño final de la cadena resultante
     * y aloja de manera dinámica el buffer exacto en el Heap de SQLite, previniendo errores comunes de
     * desbordamiento de buffer (`Buffer Overflow`).
     *
     * @param[in] format Cadena de plantilla que define las especificaciones de formato.
     * @param[in] ...    Lista de argumentos variables correspondiente a los tokens de la plantilla.
     *
     * @return char* Puntero a la nueva cadena de texto formateada y terminada en cero.
     * @retval NULL  Si el parámetro @p format es nulo o si ocurrió un fallo crítico de asignación de memoria.
     *
     * @note **Soporte de Tokens Seguros de SQLite:** Además de los especificadores estándar de C (como `%s`, `%d`),
     *       esta función soporta nativamente tokens de escape seguro como **`%q`** (que duplica automáticamente
     *       las comillas simples previniendo Inyecciones SQL) y **`%Q`** (que formatea el valor entre comillas simples
     *       y traduce punteros nulos al literal de base de datos `NULL`).
     * @note **Gestión de Memoria:** El llamador **debe** liberar de forma obligatoria la memoria de la cadena
     *       retornada invocando la función destructora @ref SqlLite_Core_FreeString.
     *
     * @see SqlLite_Core_FreeString
     * @see SqlLite_Core_Concat
     * @see https://sqlite.org
     */
    char *SqlLite_Core_Format(const char *format, ...);

    // ============================================================================
    // LIBERACION DE CADENAS
    // ============================================================================

    /**
     * @brief Libera la memoria de una cadena de texto asignada dinámicamente por el motor de SQLite.
     *
     * Esta función envuelve de forma segura el liberador nativo `sqlite3_free()`, realizando una
     * validación previa para verificar que el puntero no sea nulo. Se utiliza principalmente para
     * destruir los buffers de texto generados de manera transparente por las funciones de conversión
     * de codificación del módulo.
     *
     * @param[in] input Puntero a la cadena de caracteres cuya memoria en el Heap se desea liberar.
     *
     * @note Si el parámetro @p input es un puntero nulo (`NULL`), la función retorna inmediatamente
     *       sin realizar ninguna operación, previniendo fallos por punteros inválidos.
     *
     * @see SqlLite_Core_AnsiToUTF8
     * @see SqlLite_Core_UTF8ToAnsi
     */
    void SqlLite_Core_FreeString(const char *input);

    // ============================================================================
    // TRATAMIENTO ANSI-UT8 DE CADENAS
    // ============================================================================

    /**
     * @brief Detecta de forma analítica la codificación de una cadena de texto terminada en cero.
     *
     * Examina byte por byte la cadena provista para determinar si cumple con las reglas del estándar
     * ASCII puro, si implementa secuencias de bytes Unicode válidas (UTF-8 de 2 a 4 bytes), o si por
     * el contrario contiene caracteres extendidos bajo una página de códigos local (ANSI / Windows-1252).
     *
     * @param[in] str Puntero a la cadena de texto de entrada que se va a analizar.
     *
     * @return int Constante numérica que identifica el tipo de codificación detectado.
     * @retval ENCODING_ASCII Si la cadena contiene exclusivamente bytes en el rango de 0 a 127 (7 bits).
     * @retval ENCODING_UTF8  Si se detectaron bytes superiores a 127 pero todos cumplen estrictamente
     *                        con el patrón matemático de continuación Unicode.
     * @retval ENCODING_ANSI  Si se detectó un byte alto que no coincide con la estructura de un
     *                        carácter de inicio o de continuación válido de UTF-8.
     *
     * @note Si el parámetro @p str es un puntero nulo, la función retorna de manera segura `ENCODING_ASCII`.
     */
    int SqlLite_Core_DetectEncoding(const char *str);

    /**
     * @brief Convierte una cadena codificada en formato ANSI (Windows-1252) hacia una nueva cadena en UTF-8.
     *
     * Analiza la codificación original utilizando @ref SqlLite_Core_DetectEncoding. Si el texto ya es
     * ASCII o UTF-8, duplica la cadena usando `sqlite3_mprintf`. En entornos Windows, si la cadena es
     * ANSI pura, realiza una conversión de doble paso (ANSI -> UTF-16 -> UTF-8) reservando la memoria
     * final en el Heap de SQLite.
     *
     * @param[in] ansi_str Puntero a la cadena de caracteres ANSI original de origen.
     *
     * @return char* Puntero a la nueva cadena de texto convertida a UTF-8 y terminada en cero.
     * @retval NULL  Si @p ansi_str es nulo, o si ocurrió una falla crítica de conversión o de memoria.
     *
     * @note **Gestión de Memoria:** La cadena resultante se asigna dinámicamente mediante el motor de SQLite
     *       (`sqlite3_malloc` o `sqlite3_mprintf`), por lo que el llamador **debe** liberar este recurso de forma
     *       obligatoria invocando la función destructora del módulo (como `SqlLite_Core_FreeString`).
     *
     * @see SqlLite_Core_DetectEncoding
     * @see SqlLite_Core_UTF8ToAnsi
     */
    char *SqlLite_Core_AnsiToUTF8(const char *ansi_str);

    /**
     * @brief Convierte una cadena codificada en formato UTF-8 proveniente de SQLite hacia una nueva cadena ANSI.
     *
     * Diseñada para mantener la compatibilidad hacia atrás con entornos legados de escritorio (tales como
     * Microsoft Visual FoxPro) que no interpretan Unicode de forma nativa. Si la cadena original no requiere
     * conversión (ASCII o ANSI), se clona de inmediato de manera segura. En Windows, realiza una descompresión
     * intermedia (UTF-8 -> UTF-16 -> ANSI de la página de códigos del sistema).
     *
     * @param[in] utf8_str Puntero a la cadena de caracteres en formato UTF-8 original.
     *
     * @return char* Puntero a la nueva cadena de texto convertida a ANSI y terminada en cero.
     * @retval NULL  Si @p utf8_str es nulo, o si falló el proceso de mapeo de caracteres.
     *
     * @note **Gestión de Memoria:** Al igual que su contraparte, la memoria devuelta se aloja en el Heap mediante
     *       el gestor interno de SQLite. Es responsabilidad estricta del llamador destruirla para evitar fugas de RAM.
     *
     * @see SqlLite_Core_DetectEncoding
     * @see SqlLite_Core_AnsiToUTF8
     */
    char *SqlLite_Core_UTF8ToAnsi(const char *utf8_str);

    // ============================================================================
    // MANEJO DE ERRORES
    // ============================================================================

    /**
     * @brief Obtiene el código de error primario o base de la última operación fallida.
     *
     * Retorna el código de error estándar de 8 bits generado por la llamada más reciente a la API
     * de SQLite en el manejador especificado.
     *
     * @param[in] db Puntero al manejador activo de la base de datos (`sqlite3`).
     *
     * @return int Código de error primario nativo de SQLite (ej: `SQLITE_SCHEMA`, `SQLITE_CONSTRAINT`).
     * @retval 0   Si el manejador @p db provisto es nulo.
     *
     * @see SqlLite_Core_GetLastErrorCodeExtended
     * @see SqlLite_Core_GetLastErrorMsg
     * @see https://sqlite.org
     */
    int SqlLite_Core_GetLastErrorCode(sqlite3 *db);

    /**
     * @brief Obtiene el código de error extendido de la última operación fallida.
     *
     * Proporciona un nivel de detalle superior al código de error base, combinando el código
     * primario con un modificador de alta precisión (ej: transforma un genérico `SQLITE_CONSTRAINT`
     * en un detallado `SQLITE_CONSTRAINT_FOREIGNKEY`).
     *
     * @param[in] db Puntero al manejador activo de la base de datos (`sqlite3`).
     *
     * @return int Código de error extendido nativo de SQLite.
     * @retval 0   Si el manejador @p db provisto es nulo.
     *
     * @see SqlLite_Core_GetLastErrorCode
     * @see SqlLite_Core_GetLastErrorMsg
     * @see https://sqlite.org
     */
    int SqlLite_Core_GetLastErrorCodeExtended(sqlite3 *db);

    /**
     * @brief Obtiene el mensaje descriptivo en texto del último error ocurrido.
     *
     * Retorna una cadena de caracteres legible en formato UTF-8 que detalla la causa exacta
     * de la falla más reciente en el hilo del manejador de base de datos.
     *
     * @param[in] db Puntero al manejador activo de la base de datos (`sqlite3`).
     *
     * @return const char* Puntero a la cadena de texto con la descripción del error.
     * @retval "" (Cadena vacía) Si el manejador @p db provisto es nulo.
     *
     * @note La memoria del mensaje devuelto es administrada internamente por SQLite y no debe ser liberada.
     * @see SqlLite_Core_GetLastErrorCode
     */
    const char *SqlLite_Core_GetLastErrorMsg(sqlite3 *db);

    // ============================================================================
    // APERTURA Y CIERRE DE CONEXION
    // ============================================================================

    /**
     * @internal
     * @brief USO INTERNO. NO EXPUESTO.
     * Función extendida de apertura de bases de datos con validación estricta de banderas.
     *
     * Realiza una apertura segura de una base de datos SQLite evaluando la combinación de banderas
     * de acceso para mitigar errores en tiempo de ejecución. Si ocurre un fallo tras la asignación
     * parcial del manejador, garantiza la liberación de recursos llamando a `sqlite3_close_v2()`.
     *
     * @param[in]  uripath Puntero a la cadena con la ruta del archivo o URI de la base de datos.
     * @param[in]  flags   Máscara de bits con las banderas de configuración de SQLite.
     * @param[out] out_rc  Puntero opcional donde se almacenará el código de resultado nativo de SQLite.
     *
     * @return sqlite3* Puntero al manejador de la base de datos abierta exitosamente.
     * @retval NULL     Si los argumentos son inválidos (`SQLITE_MISUSE`) o si la apertura falló.
     *
     * @see SqlLite_Core_Close
     */
    sqlite3 *SqlLite_Core_OpenEx(
        const char *uripath,
        int flags,
        int *out_rc);

    /**
     * @brief Abre una conexión a la base de datos en modo exclusivo de sólo lectura.
     *
     * @param[in]  uripath Ruta o URI del archivo de base de datos.
     * @param[out] out_rc  Puntero opcional para recibir el código de resultado de SQLite.
     * @return sqlite3*    Puntero al manejador creado, o `NULL` en caso de error.
     */
    sqlite3 *SqlLite_Core_Open_ReadOnly(
        const char *uripath,
        int *out_rc);

    /**
     * @brief Abre una conexión en modo lectura/escritura y crea el archivo si no existe.
     *
     * @param[in]  uripath Ruta o URI del archivo de base de datos.
     * @param[out] out_rc  Puntero opcional para recibir el código de resultado de SQLite.
     * @return sqlite3*    Puntero al manejador creado, o `NULL` en caso de error.
     */
    sqlite3 *SqlLite_Core_Open_Or_Create(
        const char *uripath,
        int *out_rc);

    /**
     * @brief Abre una conexión a la base de datos en modo lectura/escritura (falla si el archivo no existe).
     *
     * @param[in]  uripath Ruta o URI del archivo de base de datos.
     * @param[out] out_rc  Puntero opcional para recibir el código de resultado de SQLite.
     * @return sqlite3*    Puntero al manejador creado, o `NULL` en caso de error.
     */
    sqlite3 *SqlLite_Core_Open(
        const char *uripath,
        int *out_rc);

    /**
     * @brief Habilita el soporte global de URIs y abre una conexión en modo de sólo lectura.
     *
     * @param[in]  uripath URI del archivo de base de datos (ej: "file:data.db?mode=ro").
     * @param[out] out_rc  Puntero opcional para recibir el código de resultado de SQLite.
     * @return sqlite3*    Puntero al manejador creado, o `NULL` en caso de error.
     */
    sqlite3 *SqlLite_Core_Open_Uri_ReadOnly(const char *uripath, int *out_rc);

    /**
     * @brief Cierra una conexión activa a la base de datos de forma segura.
     *
     * Invoca de forma interna a `sqlite3_close_v2()`, lo que previene bloqueos del tipo
     * `SQLITE_BUSY` si existen sentencias preparadas o cursores abiertos que no fueron
     * finalizados explícitamente en la capa de la aplicación.
     *
     * @param[in] db Puntero al manejador de la base de datos (`sqlite3`) que se desea cerrar.
     *
     * @retval 1  Si la base de datos se cerró exitosamente (`SQLITE_OK`).
     * @retval 0  Si el cierre falló o el manejador sigue retenido de forma asíncrona.
     * @retval SQLITE_MISUSE Si el parámetro @p db provisto es nulo.
     */
    int SqlLite_Core_Close(sqlite3 *db);

    // ============================================================================
    // OPCIONES U SETEOS DE CONEXION
    // ============================================================================

    /**
     * @brief Verifica si la base de datos especificada fue abierta en modo de sólo lectura.
     *
     * @param[in] db Puntero al manejador activo de la base de datos.
     *
     * @retval 1  Si el esquema interno "main" es de sólo lectura de forma efectiva.
     * @retval 0  Si la base de datos tiene permisos de escritura asignados.
     * @retval -1 Si el manejador @p db provisto es inválido o nulo.
     */
    int SqlLite_Core_IsReadonly(sqlite3 *db);

    /**
     * @brief Configura el tiempo de espera máximo si la base de datos se encuentra bloqueada.
     *
     * Limita los parámetros de reintento entre 0 y el valor tope definido en la constante
     * `@ref CORE_MAX_BUSY_TIMEOUT` para evitar bloqueos prolongados o hilos colgados en
     * ambientes con alta concurrencia.
     *
     * @param[in] db Puntero al manejador activo de la base de datos.
     * @param[in] ms Tiempo máximo de espera tolerado expresado en milisegundos.
     *
     * @retval 1  Si la configuración del timeout se aplicó correctamente.
     * @retval 0  Si el motor de base de datos rechazó el ajuste de concurrencia.
     * @retval SQLITE_MISUSE Si el manejador @p db provisto es nulo.
     *
     * @see https://sqlite.org
     */
    int SqlLite_Core_SetBusyTimeout(sqlite3 *db, int ms);

    // ============================================================================
    // PARAMETROS OUTPUT / INPUTOUTPUT
    // ============================================================================

    /**
     * @brief Remueve de forma segura el carácter de prefijo de un parámetro de SQLite si está presente.
     *
     * Los parámetros con nombre en SQLite pueden comenzar con los caracteres ':', '$', '@' o '?'.
     * Esta función utiliza aritmética de punteros para omitir dicho símbolo inicial de manera eficiente,
     * sin asignar nueva memoria en el Heap ni modificar la cadena de texto original.
     *
     * @param[in] name Puntero a la cadena de texto con el nombre en bruto (raw) del parámetro SQLite.
     *
     * @return const char* Puntero a la cadena de texto limpia sin el prefijo.
     * @retval NULL Si el parámetro @p name es nulo.
     * @retval name Retorna el mismo puntero original si la cadena no contiene ningún prefijo válido.
     *
     * @note Si la cadena tiene una longitud menor o igual a 1 carácter, se devuelve el puntero original sin cambios.
     */
    const char *SqlLite_Core_CleanPrefix(const char *name);

    /**
     * @internal
     * @brief USO INTERNO. NO EXPUESTO.
     *
     * Vincula de forma dinámica y segura los parámetros de usuario a una sentencia SQL compilada.
     *
     * Esta función recorre los parámetros requeridos por una sentencia SQLite preparada y busca
     * coincidencias en el arreglo de parámetros provisto por el usuario. Soporta búsquedas
     * con o sin distinción de mayúsculas (Case-Sensitive/Insensitive), omite parámetros de
     * salida, realiza conversión transparente de codificación ANSI a UTF-8 para textos y evita
     * riesgos de desbordamiento de pila (Stack Overflow).
     *
     * @param[in,out] stmt       Puntero a la sentencia preparada de SQLite (`sqlite3_stmt`).
     * @param[in]     params     Arreglo de estructuras `SqlLiteCoreParam` con los datos a vincular.
     * @param[in]     paramCount Cantidad de elementos contenidos en el arreglo @p params.
     *
     * @note Los parámetros puramente posicionales (sin nombre, ej: '?') son omitidos por esta función.
     * @note Utiliza `SQLITE_TRANSIENT` internamente para garantizar que SQLite duplique los buffers de texto y blobs de forma segura.
     *
     * @see SqlLite_Core_CleanPrefix
     * @see SqlLite_Core_AnsiToUTF8
     */
    void SqlLite_Core_BindParameters(
        sqlite3_stmt *stmt,
        SqlLiteCoreParam *params,
        int paramCount);

    /**
     * @internal
     * @brief USO INTERNO. NO EXPUESTO.
     *
     * Extrae los resultados de la base de datos hacia los parámetros con dirección OUTPUT o INPUTOUTPUT.
     *
     * Esta función inspecciona las columnas devueltas por la fila actual en una sentencia ejecutada
     * y mapea sus valores directamente hacia el arreglo de parámetros provisto por el usuario.
     * Realiza una búsqueda de nombres inmune a desbordamientos de pila (Stack Overflow) y soporta
     * lógica tanto Case-Sensitive como Case-Insensitive. Está diseñada específicamente para capturar
     * valores devueltos por cláusulas `RETURNING` o consultas `SELECT`.
     *
     * @param[in]     stmt       Puntero a la sentencia ejecutada de SQLite (`sqlite3_stmt`).
     * @param[in,out] params     Arreglo de estructuras `SqlLiteCoreParam` donde se volcarán los resultados.
     * @param[in]     paramCount Cantidad de elementos contenidos en el arreglo @p params.
     *
     * @note Realiza una conversión automática y transparente de codificación UTF-8 a ANSI si el motor
     *       devuelve texto con caracteres extendidos, garantizando compatibilidad con entornos legados.
     * @note Para tipos de datos de texto (`SQLITE_TEXT`), la función realiza una copia profunda segura
     *       validando estrictamente que el tamaño del dato no supere el límite máximo (`max_len`) del buffer destino.
     *
     * @see SqlLite_Core_CleanPrefix
     * @see SqlLite_Core_UTF8ToAnsi
     * @see SqlLite_Core_FreeString
     */
    void SqlLite_Core_ProcessOutputParameters(
        sqlite3_stmt *stmt,
        SqlLiteCoreParam *params,
        int paramCount);

    // ============================================================================
    // COMMANDS - TRANSACTIONS - INTERNALS
    // ============================================================================

    /**
     * @internal
     * @brief Intercepta comandos de control transaccional explícitos inyectados en cadenas de texto SQL.
     *
     * Filtra la presencia de palabras clave transaccionales nativas (`BEGIN`, `COMMIT`, `ROLLBACK`, `END`)
     * al inicio de la cadena de consulta (omitiendo los espacios en blanco iniciales). Actúa como un escudo
     * arquitectónico para obligar a los desarrolladores a consumir la API de transacciones explícita del
     * módulo (como @ref SqlLite_Core_BeginTransaction) en lugar de inyectar texto plano que rompa el estado
     * del modo Autocommit.
     *
     * @param[in] sql Puntero a la cadena de consulta SQL que se va a inspeccionar.
     *
     * @retval 1 Si detecta que la consulta inicia con un comando transaccional prohibido.
     * @retval 0 Si la cadena es segura para su ejecución regular o si @p sql es nulo.
     *
     * @note Para la palabra clave `END`, realiza una validación de frontera léxica estricta para no
     *       confundirla con nombres de columnas o funciones que comiencen con esas letras.
     *
     * @see SqlLite_Core_BeginTransaction
     * @see SqlLite_Core_Commit
     * @see SqlLite_Core_Rollback
     */
    int SqlLite_Core_IsForbiddenTransactionalText(const char *sql);

    /**
     * @internal
     * @brief Verifica de forma analítica si una cadena de texto contiene una única sentencia SQL.
     *
     * Examina la cadena para detectar la presencia de múltiples comandos delimitados por puntos y comas (`;`).
     * Implementa una máquina de estados simple para omitir los caracteres `;` que residen de forma legítima
     * dentro de cadenas de texto literales (`'...'`). Si localiza un punto y coma fuera de comillas, valida
     * si el texto restante contiene comandos reales o solo espacios vacíos.
     *
     * @param[in] sql Puntero a la cadena de texto que contiene la consulta SQL a evaluar.
     *
     * @retval 1 Si el flujo representa una sola sentencia (o termina limpiamente en un punto y coma).
     * @retval 0 Si @p sql es nulo o si se detectó una estructura multi-sentencia (múltiples comandos).
     *
     * @note Esta función está declarada como `static inline` para maximizar el rendimiento al evitar la
     *       sobrecarga de llamadas en el flujo crítico de preparación de consultas.
     *
     * @see SqlLite_Core_IsValidScalarCommand
     */
    int SqlLite_Core_IsSingleStatement(const char *sql);

    /**
     * @internal
     * @brief Evalúa si el comando SQL es una operación escalar segura para el retorno de datos unarios.
     *
     * Determina si la consulta cumple con las reglas gramaticales para devolver una única fila/columna.
     * Para ser considerado válido, debe pasar la verificación de @ref SqlLite_Core_IsSingleStatement y
     * corresponder exclusivamente a uno de los siguientes escenarios:
     * 1. Una sentencia `SELECT` tradicional (validando límites léxicos de la palabra clave).
     * 2. Una sentencia `INSERT` modificada que implemente de forma explícita una cláusula `RETURNING`
     *    fuera de literales de texto para extraer llaves primarias o valores autogenerados.
     *
     * @param[in] sql Puntero a la cadena de texto con la consulta SQL.
     *
     * @retval 1 Si la sentencia es un comando escalar válido y seguro (`SELECT` o `INSERT ... RETURNING`).
     * @retval 0 Si el comando viola la regla de sentencia única o no califica como operación escalar.
     *
     * @see SqlLite_Core_IsSingleStatement
     */
    int SqlLite_Core_IsValidScalarCommand(const char *sql);

    // ============================================================================
    // COMMANDS
    // ============================================================================

    /**
     * @brief Ejecuta un script SQL masivo o comandos de conveniencia sin preparación de parámetros.
     *
     * Esta función utiliza internamente el mecanismo nativo 'sqlite3_exec'. Está diseñada
     * exclusivamente para operaciones estructurales masivas en lote (Batch), tales como
     * la ejecución de scripts de migración, inicialización de esquemas (creación de múltiples
     * tablas consecutivas) o configuraciones globales del motor (PRAGMAs).
     *
     * ⚠️ ADVERTENCIA DE SEGURIDAD (INYECCIÓN SQL):
     * Al procesar cadenas de texto directas sin soporte para parámetros vinculados (bind), esta
     * función es vulnerable a ataques de inyección SQL si se construye mediante concatenación de
     * strings dinámicos. Úsela únicamente con sentencias SQL estáticas, seguras o predefinidas.
     *
     * COMPORTAMIENTO MULTI-SENTENCIA:
     * A diferencia de las funciones basadas en statements preparados (NonQuery, Scalar, Reader),
     * esta es la única función del Core que admite legítimamente múltiples instrucciones SQL separadas
     * por punto y coma (;) dentro de la misma cadena de texto.
     *
     * GESTIÓN DE MEMORIA INTEGRADA:
     * La función encapsula de forma segura la gestión de memoria de los mensajes de error dinámicos
     * generados por SQLite (zErrMsg). Si la operación falla, copia el mensaje al buffer del usuario
     * y garantiza la liberación absoluta de la memoria del motor (sqlite3_free) bajo cualquier flujo,
     * mitigando el riesgo de fugas de memoria (Memory Leaks).
     *
     * @param db            Puntero activo y válido a la conexión de la base de datos SQLite.
     * @param sql_command   Cadena de texto con el script o comandos SQL a ejecutar (Puede contener múltiples sentencias).
     * @param err_buf       [Opcional] Buffer para capturar el mensaje de error detallado del motor. Puede ser NULL.
     * @param err_buf_max   Tamaño máximo asignado al buffer de error ('err_buf').
     *
     * @return int Código de estado de la operación nativo de SQLite:
     *         - SQLITE_OK (0): Todo el script o comandos se ejecutaron con éxito total.
     *         - SQLITE_MISUSE (21): Mal uso de la librería (Punteros esenciales 'db' o 'sql_command' nulos).
     *         - Cualquier otro código de error nativo de SQLite (Ej. SQLITE_ERROR, SQLITE_BUSY, SQLITE_SYNTAX).
     *           Si ocurre un error en un script multi-sentencia, la ejecución se detiene inmediatamente
     *           en la instrucción que falló y las sentencias posteriores no se procesan.
     */
    int SqlLite_Core_ExecuteCommand(
        sqlite3 *db,
        const char *sql_command,
        char *err_buf,
        int err_buf_max);

    /**
     * @brief Ejecuta una sentencia SQL de acción que no devuelve un conjunto de filas.
     *
     * Esta función está diseñada para procesar comandos de manipulación de datos (DML) como
     * INSERT, UPDATE y DELETE, así como comandos de definición de datos (DDL) como CREATE,
     * DROP y ALTER.
     *
     * COMPORTAMIENTO SIN PARÁMETROS:
     * Si se omiten los parámetros de entrada (params = NULL y paramCount = 0), la función
     * opera de forma segura en modo "Texto Plano", saltándose la fase de vinculación (bind)
     * sin penalizaciones de rendimiento. Esto la hace ideal para comandos DDL planos directos.
     *
     * COMPORTAMIENTO CON COMANDOS DDL (CREATE / DROP):
     * Las operaciones que alteran el esquema estructural de la base de datos (como crear o
     * eliminar tablas) se procesarán con éxito pero devolverán legítimamente un valor de 0,
     * ya que no modifican registros físicos de datos.
     *
     * FILTRO TRANSACCIONAL:
     * Por motivos de seguridad transaccional, la función deniega la ejecución si el texto SQL
     * contiene comandos explícitos de control de transacciones (BEGIN, COMMIT, ROLLBACK).
     * Obliga al desarrollador a utilizar las funciones de infraestructura del Core dedicadas.
     *
     * @param db           Puntero activo y válido a la conexión de la base de datos SQLite.
     * @param sql          Cadena de texto con la sentencia SQL a ejecutar (Debe ser una sola instrucción).
     * @param params       [Opcional] Arreglo de estructuras SqlLiteCoreParam para vinculación de entrada/salida. Puede ser NULL.
     * @param paramCount   Cantidad de elementos en el arreglo 'params'. Puede ser 0.
     * @param err_buf      [Opcional] Buffer para capturar el mensaje de error detallado del motor. Puede ser NULL.
     * @param err_buf_max  Tamaño máximo asignado al buffer de error ('err_buf').
     *
     * @return int Cantidad de filas afectadas o código de error:
     *         - Valor >= 0: Operación exitosa. Devuelve el número de filas modificadas, insertadas o
     *           eliminadas (0 para comandos DDL o consultas sin impacto físico).
     *         - -1: Ocurrió un error en la fase de ejecución (step) del comando (ej. violación de restricciones).
     *           El detalle se vuelca en 'err_buf'.
     *         - SQLITE_MISUSE (21): Mal uso de la librería (Punteros esenciales nulos, comandos múltiples
     *           detectados, o transacciones prohibidas en texto plano).
     *         - Cualquier otro código de error nativo de SQLite surgido durante la compilación (prepare).
     */
    int SqlLite_Core_ExecuteNonQuery(
        sqlite3 *db,
        const char *sql,
        SqlLiteCoreParam *params,
        int paramCount,
        char *err_buf,
        int err_buf_max);

    /**
     * @brief Ejecuta una consulta SQL diseñada para retornar un único valor (Escalar).
     *
     * Esta función está optimizada para consultas de agregación (COUNT, MAX, SUM),
     * verificaciones de existencia, o sentencias de inserción/modificación modernas
     * que utilicen la cláusula RETURNING (ej. INSERT ... RETURNING id).
     *
     * COMPORTAMIENTO SIN PARÁMETROS:
     * Si se omiten los parámetros de entrada (params = NULL y paramCount = 0), la función
     * opera de forma segura en modo "Texto Plano", saltándose la fase de vinculación (bind)
     * sin penalizaciones de rendimiento.
     *
     * COMPORTAMIENTO SIN ESTRUCTURA DE SALIDA (DESCARTE):
     * Si no se requiere capturar el valor devuelto por la base de datos, el parámetro
     * 'out_scalar_result' puede enviarse como NULL. La función redirigirá internamente los
     * datos a un buffer temporal en el Stack (Dummy Buffer) para evitar desbordamientos,
     * destruyendo el resultado de forma segura al finalizar y devolviendo únicamente
     * el código de estado en el 'return'.
     *
     * COMPORTAMIENTO MULTIPLATAFORMA (ENCODING):
     * Si la base de datos devuelve un tipo de dato texto (SQLITE_TEXT), la función detecta
     * automáticamente si viene codificado en UTF-8 alto y realiza una conversión transparente
     * a formato ANSI antes de escribirlo en el buffer del usuario.
     *
     * @param db                 Puntero activo y válido a la conexión de la base de datos SQLite.
     * @param sql                Cadena de texto con la sentencia SQL (Debe ser una sola instrucción).
     * @param params             [Opcional] Arreglo de estructuras SqlLiteCoreParam para vinculación de entrada. Puede ser NULL.
     * @param paramCount         Cantidad de elementos en el arreglo 'params'. Puede ser 0.
     * @param out_scalar_result  [Opcional] Puntero a la estructura donde se depositará el tipo y valor de la primera celda. Puede ser NULL.
     * @param err_buf            [Opcional] Buffer para capturar el mensaje de error detallado del motor. Puede ser NULL.
     * @param err_buf_max        Tamaño máximo asignado al buffer de error ('err_buf').
     *
     * @return int Código de estado de la operación:
     *         - SQLITE_OK (0): La consulta se ejecutó con éxito y se procesó el resultado (si existía).
     *         - SQLITE_MISUSE (21): Mal uso de la librería (Punteros esenciales nulos, comandos múltiples detectados, o SQL inválido para un escalar).
     *         - Cualquier otro código de error nativo de SQLite (Ej. SQLITE_ERROR, SQLITE_BUSY, SQLITE_CORRUPT).
     */
    int SqlLite_Core_ExecuteScalar(
        sqlite3 *db,
        const char *sql,
        SqlLiteCoreParam *params,
        int paramCount,
        SqlLiteCoreParam *out_scalar_result,
        char *err_buf,
        int err_buf_max);

    /**
     * @brief Ejecuta una consulta SQL de lectura avanzada (SELECT) procesando las filas mediante streaming.
     * @param db               Puntero activo a la base de datos SQLite.
     * @param sql              Cadena de texto SQL en formato ANSI.
     * @param params           [Opcional] Arreglo de estructuras con parámetros de entrada. Puede ser NULL.
     * @param paramCount       Cantidad de elementos en el arreglo de parámetros. Puede ser 0.
     * @param reader_callback  Función callback estandarizada (__cdecl) para procesar cada fila.
     * @param pUserContext     Puntero al contexto personalizado del desarrollador (instancia, estructura, etc.).
     * @param err_buf          Buffer para capturar el mensaje detallado en caso de error.
     * @param err_buf_max      Tamaño máximo asignado al buffer de error.
     * @return int             Cantidad de filas procesadas con éxito (0 o más), o un código de error nativo de SQLite.
     */
    int SqlLite_Core_ExecuteReader(
        sqlite3 *db,
        const char *sql,
        SqlLiteCoreParam *params,
        int paramCount,
        SqlLiteCoreRowCallback reader_callback,
        void *pUserContext,
        char *err_buf, int err_buf_max);

    // ============================================================================
    // TRANSACCIONES
    // ============================================================================

    /**
     * @internal
     * @brief USO INTERNO. NO EXPUESTO EN LA API PÚBLICA.
     *
     * Centraliza la ejecución de comandos transaccionales (`BEGIN`, `COMMIT`, `ROLLBACK`)
     * validando el estado actual del motor mediante el modo Autocommit de SQLite.
     *
     * @param[in]  db           Puntero al manejador activo de la base de datos (`sqlite3`).
     * @param[in]  sql_command  Cadena de texto con el comando SQL a ejecutar de forma nativa.
     * @param[in]  txexpected   Estado esperado de Autocommit para validar la acción (1 o 0).
     * @param[out] err_buf      Buffer de destino opcional para capturar el mensaje de error si ocurre una falla.
     * @param[in]  err_buf_max  Tamaño máximo en bytes asignado al buffer @p err_buf.
     *
     * @return int Código de resultado de la operación.
     * @retval  0 (`SQLITE_OK`) Si la transacción se procesó con éxito.
     * @retval -1 Si el estado actual de Autocommit no coincide con @p txexpected (discrepancia lógica).
     * @retval N  Código de error nativo devuelto por el motor SQLite en caso de fallo en la ejecución.
     *
     * @note **Lógica de Validación (Autocommit vs Expectativa):**
     * - **`txexpected = 1`** (@ref SqlLite_Core_BeginTransaction): El estado esperado debe ser 1 (Libre).
     *   Si es 0 (Transacción activa), intercepta el doble `BEGIN` secuencial y aborta devolviendo -1.
     * - **`txexpected = 0`** (@ref SqlLite_Core_Commit / @ref SqlLite_Core_Rollback): El estado esperado debe ser 0 (Activo).
     *   Si es 1 (Libre), detecta la anomalía de un comando huérfano y aborta devolviendo -1 sin inyectarlo al motor.
     */
    int SqlLite_Core_ExecuteTransaction(
        sqlite3 *db,
        const char *sql_command,
        int txexpected,
        char *err_buf,
        int err_buf_max);

    /**
     * @brief Inicia una nueva transacción explícita en la base de datos de forma segura.
     *
     * Invoca internamente a @ref SqlLite_Core_ExecuteTransaction configurando la expectativa de
     * Autocommit en estado libre (`txexpected = 1`). Si detecta que ya existe una transacción en curso,
     * aborta de manera inmediata para evitar errores lógicos de anidamiento no soportados nativamente.
     *
     * @param[in]  db           Puntero al manejador activo de la base de datos.
     * @param[out] err_buf      Buffer opcional para almacenar la descripción del error.
     * @param[in]  err_buf_max  Capacidad máxima en bytes del buffer @p err_buf.
     *
     * @return int Código de resultado de la operación (0 para éxito, -1 por doble transacción o código nativo SQLite).
     *
     * @see SqlLite_Core_ExecuteTransaction
     * @see SqlLite_Core_Commit
     * @see SqlLite_Core_Rollback
     */
    int SqlLite_Core_BeginTransaction(sqlite3 *db, char *err_buf, int err_buf_max);

    /**
     * @brief Confirma y consolida de forma permanente los cambios de la transacción actual en el disco.
     *
     * Invoca internamente a @ref SqlLite_Core_ExecuteTransaction configurando la expectativa de
     * Autocommit en estado activo (`txexpected = 0`). Si se invoca de forma huérfana sin haber abierto
     * una transacción previa, el validador frena la ejecución devolviendo -1 para proteger la sesión.
     *
     * @param[in]  db           Puntero al manejador activo de la base de datos.
     * @param[out] err_buf      Buffer opcional para almacenar la descripción del error.
     * @param[in]  err_buf_max  Capacidad máxima en bytes del buffer @p err_buf.
     *
     * @return int Código de resultado de la operación (0 para éxito, -1 por llamada huérfana o código nativo SQLite).
     *
     * @see SqlLite_Core_ExecuteTransaction
     * @see SqlLite_Core_BeginTransaction
     * @see SqlLite_Core_Rollback
     */
    int SqlLite_Core_Commit(sqlite3 *db, char *err_buf, int err_buf_max);

    /**
     * @brief Revierte y deshace todas las operaciones de modificación realizadas en la transacción actual.
     *
     * Invoca internamente a @ref SqlLite_Core_ExecuteTransaction configurando la expectativa de
     * Autocommit en estado activo (`txexpected = 0`). Restaura la base de datos al estado exacto
     * previo al inicio del bloque transaccional. Si no hay cambios que revertir, frena la ejecución devolviendo -1.
     *
     * @param[in]  db           Puntero al manejador activo de la base de datos.
     * @param[out] err_buf      Buffer opcional para almacenar la descripción del error.
     * @param[in]  err_buf_max  Capacidad máxima en bytes del buffer @p err_buf.
     *
     * @return int Código de resultado de la operación (0 para éxito, -1 por llamada inválida o código nativo SQLite).
     *
     * @see SqlLite_Core_ExecuteTransaction
     * @see SqlLite_Core_BeginTransaction
     * @see SqlLite_Core_Commit
     */
    int SqlLite_Core_Rollback(sqlite3 *db, char *err_buf, int err_buf_max);

    // ============================================================================
    // SQL BLOB
    // ============================================================================

    /**
     * Extrae un objeto binario grande (BLOB) de SQLite directamente a un búfer de memoria preasignado.
     * <p>
     * Esta función ejecuta una consulta SQL secuencial de un solo paso orientada al máximo 
     * rendimiento (Zero-Copy). Recupera el puntero físico del búfer interno del motor y 
     * realiza una transferencia lineal de bytes mediante instrucciones optimizadas de CPU.
     * Incluye mecanismos estrictos de control para prevenir desbordamientos de búfer (Buffer Overflow).
     * </p>
     *
     * @param db           Puntero activo y válido a la estructura de conexión de SQLite (sqlite3).
     * @param sql          Cadena de caracteres constante que contiene la sentencia SQL a ejecutar.
     * @param pDestBuffer  Puntero al bloque de memoria física preasignado donde se copiarán los bytes del BLOB.
     * @param maxLen       Capacidad física máxima expresada en bytes asignada al búfer de destino.
     * @param pOutCopied   Puntero a un entero donde se escribirá la cantidad exacta de bytes transferidos.
     * 
     * @return Un entero que actúa como código de estado de la operación:
     *         <ul>
     *           <li><b>0 (SQLITE_OK):</b> Operación exitosa (datos copiados o conjunto de resultados vacío).</li>
     *           <li><b>-2:</b> Error crítico. El búfer de destino es insuficiente y los datos fueron truncados.</li>
     *           <li><b>Código nativo de SQLite (p. ej., SQLITE_MISUSE):</b> Error de compilación o violación de API.</li>
     *         </ul>
     */
    int SqlLite_Core_GetBlob(
        sqlite3* db, 
        const char* sql, 
        void* pDestBuffer, 
        int maxLen, 
        int* pOutCopied);

#ifdef __cplusplus
}
#endif

#endif // SQLITE_CORE_H
