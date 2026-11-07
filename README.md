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

## 🛠️ Requisitos de Compilación

Para generar los binarios del proyecto (tanto la DLL como la FLL de FoxPro), el entorno requiere las siguientes herramientas en Windows:

* **IDE / Compilador:** [Visual Studio 2022](https://learn.microsoft.com/es-es/visualstudio/releases/2026/release-notes "Visual Studio Releases") con la carga de trabajo de desarrollo para el escritorio con C++.
* **SDK del Sistema:** [Windows 11 SDK (Versión 10.0.28000)](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads "Windows SDK Downloads").
* **Generador:** [CMake](https://learn.microsoft.com/es-es/vcpkg/get_started/get-started-vs "CMake Visual Studio Integration") (versión 3.15 o superior).

---

## 🏗️ Proceso de Build (Generación de Binarios)

Puedes compilar los componentes de manera manual siguiendo estos comandos en la terminal desde la raíz del proyecto, o bien ejecutando directamente los archivos por lotes automatizados incluidos en el repositorio: `cmp_dll.bat` y `cmp_fll.bat`.

### Paso 1: Clonar el repositorio
```bash
git clone https://github.com
cd libsqlite
```

### Paso 2: Compilar el Conector Genérico (DLL x86/Win32)
```bash
# Limpiar compilaciones previas si existen
rmdir /s /q build

# Configurar y compilar la DLL dinámica
cmake -S src/sqlite -B build -A Win32
cmake --build build --config Release
```

### Paso 3: Compilar la Extensión para FoxPro (FLL x86/Win32)
```bash
# Limpiar el directorio temporal de build
rmdir /s /q build

# Configurar y compilar la FLL nativa
cmake -S src/vfp/sqlite -B build -A Win32
cmake --build build --config Release
rmdir /s /q build
```

### 🌐 Compilación para Arquitecturas Específicas (Win32 vs x64)

Por defecto, Visual FoxPro requiere binarios de **32 bits (Win32)**.
Si deseas compilar el conector genérico (DLL) para aplicaciones modernas de **64 bits**,
 utiliza el modificador `-A x64` en CMake.

⚠️**Nota importante sobre la FLL:** La extensión de FoxPro (`src/vfp/sqlite`)
**debe permanecer siempre en `-A Win32`**. Compilar la FLL en x64 romperá la compatibilidad
 con Visual FoxPro y causará errores de carga en el IDE.

#### Generar DLL en 64 bits (x64)
```bash
rmdir /s /q build
cmake -S src/sqlite -B build -A x64
cmake --build build --config Release
rmdir /s /q build
```

## 🐧 Compilación en Linux (Solo Shared Library `.so`)

El núcleo del proyecto es compatible con entornos Linux (como Ubuntu, Debian o Fedora). Dado que los archivos FLL son exclusivos del entorno Windows y de Visual FoxPro, bajo plataformas Linux el código se compila únicamente como una biblioteca compartida estándar nativa (`libsqlite3.so`).

### Requisitos previos en Linux
Asegúrate de contar con las herramientas esenciales de desarrollo, GCC y CMake instalados:
```bash
# En sistemas basados en Ubuntu/Debian:
sudo apt update
sudo apt install build-essential cmake
```

### Proceso de Construcción
Ejecuta la secuencia estándar de CMake desde la raíz de tu proyecto para compilar la biblioteca en configuración de liberación:

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

