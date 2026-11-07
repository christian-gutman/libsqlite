# libsqlite
### SQLite Connector | Visual FoxPro Connector Library

**libsqlite** es un conector y cliente de alto rendimiento desarrollado en **C** basado directamente en el núcleo oficial de `sqlite3.c`. Está diseñado bajo una arquitectura híbrida para servir a dos propósitos principales en entornos Windows:

1. **DLL Dinámica Estándar (`.dll`):** Funciona como un conector de uso general compatible con cualquier lenguaje de programación que soporte llamadas a la API de Windows (C#, C++, Delphi, VB.NET, etc.).
2. **Biblioteca de Enlace de FoxPro (`.fll`):** Proporciona una integración 100% nativa y directa para **Visual FoxPro** mediante la instrucción `SET LIBRARY TO`.

El proyecto elimina la necesidad de depender de controladores ODBC, proveedores OLE DB o capas ADO pesadas, ofreciendo un acceso directo, ultrarrápido y portable a bases de datos SQLite.

## 🚀 Características principales

* **Núcleo Embebido:** Utiliza `sqlite3.c` internamente. No requiere instalaciones ni configuraciones previas en el sistema operativo.
* **Formato Dual Integrado:** Un único diseño de funciones mapeado tanto para exportación de DLL estándar como para el API de extensiones de FoxPro.
* **Soporte Unicode/ANSI:** Resuelve de forma nativa la conversión de codificación de texto (`UTF8ToAnsi` y `AnsiToUTF8`), asegurando la compatibilidad de caracteres especiales entre SQLite y lenguajes tradicionales.
* **Salida JSON Dinámica:** Incluye un escritor de JSON integrado (`JsonWriterCallback`) que permite transformar los resultados de las consultas en texto estructurado directamente desde el núcleo en C.
* **Ligero y Portable:** Ideal para aplicaciones que se distribuyen sin instalador (*XCOPY deployment*).

---

## 🛠️ API / Estructura del Proyecto

Las funciones están organizadas modularmente en los archivos fuente del proyecto según su rol:

### 🔌 Gestión de Conexiones (`sqlite_core_cnx`)
* `SqlLite_Core_Open`, `SqlLite_Core_Open_ReadOnly`, `SqlLite_Core_Open_Or_Create`, `SqlLite_Core_Open_Uri_ReadOnly`: Funciones optimizadas para la apertura y control de archivos de base de datos bajo diferentes directivas de acceso.
* `SqlLite_Core_Close`: Cierre seguro liberando los manejadores de SQLite.
* `SqlLite_Core_IsReadonly` / `SqlLite_Core_SetBusyTimeout`: Control de estado y tiempos de espera para bloqueos de concurrencia.

### ⚡ Ejecución de Comandos (`sqlite_core_command` / `sqlite_core_transaction`)
* `SqlLite_Core_ExecuteCommand` / `SqlLite_Core_ExecuteNonQuery`: Ejecución directa de sentencias DDL y DML.
* `SqlLite_Core_ExecuteScalar`: Recuperación rápida de valores únicos (como IDs autoincrementables o conteos).
* `SqlLite_Core_ExecuteReader`: Recuperación orientada a filas para la lectura secuencial de datos.
* `SqlLite_Core_BeginTransaction`, `SqlLite_Core_Commit`, `SqlLite_Core_Rollback`, `SqlLite_Core_ExecuteTransaction`: Control absoluto y seguro de operaciones atómicas.

### 🔤 Utilidades, Cadenas y Errores
* **Manejo de Strings (`sqlite_core_string`):** Funciones críticas de formateo, concatenación, liberación de memoria (`FreeString`) y codificación de páginas de texto.
* **Parámetros (`sqlite_core_parameter`):** Sanitización y limpieza de prefijos en variables de consulta con `CleanPrefix`.
* **Diagnóstico (`sqlite_core_error`):** Extracción de mensajes textuales (`GetLastErrorMsg`) y códigos internos extendidos (`GetLastErrorCodeExtended`).


