# libsqlite

### High-performance SQLite connector library for C

> ⚠️ **Nota de migración y estado del proyecto:** 
> Este desarrollo pertenecía originalmente a la cuenta `@christiangutman`. Lamentablemente, debido a un robo de hardware, se perdió el acceso definitivo a dicha cuenta y a su repositorio original. 
> 
> Gracias a que el código se encontraba respaldado en diferentes proveedores y servidores externos, logré recuperar las fuentes de manera íntegra. A raíz de este suceso, he tomado la firme decisión de liberar el proyecto, transformándolo en un desarrollo completamente **público y de código abierto** para la comunidad. 
> 
> A partir de ahora, **este repositorio será el hogar oficial y definitivo** para el mantenimiento, actualizaciones y evolución de **libsqlite**. ¡Las contribuciones y reportes de bugs son más que bienvenidos aquí!


**libsqlite** es un conector y cliente de alto rendimiento desarrollado en **C** basado directamente en el núcleo oficial de `sqlite3.c`. Expone una API compartida (DLL/.so) con semántica equivalente a su par en .NET (`ExecuteReader`, `ExecuteScalar`, `ExecuteNonQuery`, parámetros con dirección, transacciones), compatible con cualquier lenguaje que pueda hacer llamadas a C (C#, C++, Delphi, VB.NET, Python vía ctypes, etc.).

El proyecto elimina la necesidad de depender de controladores ODBC, proveedores OLE DB o capas ADO pesadas, ofreciendo un acceso directo, ultrarrápido y portable a bases de datos SQLite.

## 🚀 Características principales

* **Núcleo Embebido:** Utiliza `sqlite3.c` internamente. No requiere instalaciones ni configuraciones previas en el sistema operativo.
* **Semántica .NET:** La API replica el comportamiento de sus equivalentes en .NET (ADO.NET-style), reduciendo la curva de aprendizaje.
* **Soporte Unicode/ANSI:** Resuelve de forma nativa la conversión de codificación de texto (`UTF8ToAnsi` y `AnsiToUTF8`), asegurando la compatibilidad de caracteres especiales entre SQLite y lenguajes tradicionales.
* **Salida JSON Dinámica:** Incluye un escritor de JSON integrado (`JsonWriterCallback`) que permite transformar los resultados de las consultas en texto estructurado directamente desde el núcleo en C.
* **Ligero y Portable:** Ideal para aplicaciones que se distribuyen sin instalador (*XCOPY deployment*).

---

## 🛠️ API / Estructura del Proyecto

Las funciones están organizadas modularmente en los archivos fuente del proyecto según su rol:

### 🔌 Gestión de Conexiones (`sqlite_core_cnx`)
* `sqlite_core_open`, `sqlite_core_open_readonly`, `sqlite_core_open_or_create`, `sqlite_core_open_uri_readonly`: Funciones optimizadas para la apertura y control de archivos de base de datos bajo diferentes directivas de acceso.
* `sqlite_core_close`: Cierre seguro liberando los manejadores de SQLite.
* `sqlite_core_is_readonly` / `sqlite_core_set_busy_timeout`: Control de estado y tiempos de espera para bloqueos de concurrencia.

### ⚡ Ejecución de Comandos (`sqlite_core_command` / `sqlite_core_transaction`)
* `sqlite_core_execute_command` / `sqlite_core_execute_non_query`: Ejecución directa de sentencias DDL y DML.
* `sqlite_core_execute_scalar`: Recuperación rápida de valores únicos (como IDs autoincrementables o conteos).
* `sqlite_core_execute_reader`: Recuperación orientada a filas para la lectura secuencial de datos.
* `sqlite_core_begin_transaction`, `sqlite_core_commit_transaction`, `sqlite_core_rollback_transaction`, `sqlite_core_execute_transaction`: Control absoluto y seguro de operaciones atómicas.

### 🔤 Utilidades, Cadenas y Errores
* **Manejo de Strings (`sqlite_core_string`):** Funciones críticas de formateo, concatenación, liberación de memoria (`FreeString`) y codificación de páginas de texto.
* **Parámetros (`sqlite_core_parameter`):** Sanitización y limpieza de prefijos en variables de consulta con `CleanPrefix`.
* **Diagnóstico (`sqlite_core_error`):** Extracción de mensajes textuales (`GetLastErrorMsg`) y códigos internos extendidos (`GetLastErrorCodeExtended`).

## 🛠️ Requisitos de Compilación

Para generar los binarios del proyecto, el entorno requiere las siguientes herramientas en Windows:

* **IDE / Compilador:** [Visual Studio 2022](https://learn.microsoft.com/es-es/visualstudio/releases/2026/release-notes "Visual Studio Releases") con la carga de trabajo de desarrollo para el escritorio con C++.
* **SDK del Sistema:** [Windows 11 SDK (Versión 10.0.28000)](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads "Windows SDK Downloads").
* **Generador:** [CMake](https://learn.microsoft.com/es-es/vcpkg/get_started/get-started-vs "CMake Visual Studio Integration") (versión 3.15 o superior).

---

## 🏗️ Proceso de Build (Generación de Binarios)

Puedes compilar el componente de manera manual siguiendo estos comandos en la terminal desde la raíz del proyecto, o bien ejecutando directamente el archivo por lotes automatizado incluido en el repositorio: `cmp_dll.bat`.

### Paso 1: Clonar el repositorio
```bash
git clone https://github.com/christian-gutman/libsqlite.git
cd libsqlite
```

### Paso 2: Compilar la biblioteca (DLL Win32 o x64)
```bash
# Limpiar compilaciones previas si existen
rmdir /s /q build

# Configurar y compilar (Win32; usar -A x64 para 64 bits)
cmake -S src/sqlite -B build -A Win32
cmake --build build --config Release
```

## 🐧 Compilación en Linux (Solo Shared Library `.so`)

### En proceso de implementacion

El núcleo del proyecto es compatible con entornos Linux (como Ubuntu, Debian o Fedora), donde el código se compila como una biblioteca compartida estándar nativa (`libsqlite3.so`).

### Requisitos previos en Linux
Asegúrate de contar con las herramientas esenciales de desarrollo, GCC y CMake instalados:
```bash
# En sistemas basados en Ubuntu/Debian:
sudo apt update
sudo apt install build-essential cmake
```

### Proceso de Construcción
Ejecuta la secuencia estándar de CMake desde la raíz de tu proyecto para compilar la biblioteca 

```bash
# Limpiar directorios de compilación previos
rm -rf build

# Configurar el entorno de generación y compilar
cmake -S src/sqlite -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# El archivo binario generado se ubicará en la carpeta centralizada de binarios:
# bin/x64-Linux/Release/libsqlite3.so

# Opcional: Limpiar temporales de la compilación
rm -rf build
```

