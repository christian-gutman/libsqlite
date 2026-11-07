cmake_minimum_required(VERSION 3.15)
project(libsqlite3 VERSION 4.2 LANGUAGES C CXX)
 
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
 
if(NOT MSVC)
    message(FATAL_ERROR "libsqlite3 requiere arquitectura Win y compilador MSVC.")
endif()
 
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(PLATFORM_DIR "x32")
else()
    set(PLATFORM_DIR "x64")
endif()

set(ARQUITECTURE_DIR ${CMAKE_SYSTEM_PROCESSOR})
if(NOT ARQUITECTURE_DIR)
    set(ARQUITECTURE_DIR ${CMAKE_HOST_SYSTEM_NAME})
endif()

set(DIR_BUILD "${PLATFORM_DIR}-${ARQUITECTURE_DIR}")
 
set(SRC_libsqlite3
    "${CMAKE_CURRENT_SOURCE_DIR}/../core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/../core.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/../encoding/core/core_encoding.h"    
    "${CMAKE_CURRENT_SOURCE_DIR}/../encoding/core/core_encoding.c"    

    #"${CMAKE_CURRENT_SOURCE_DIR}/../strings/core/core_string.h"    
    #"${CMAKE_CURRENT_SOURCE_DIR}/../strings/core/core_string.c"    
 
    "${CMAKE_CURRENT_SOURCE_DIR}/core/engine/sqlite3.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/engine/sqlite3.c"  

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_error.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_error.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_string.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_string.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_cnx.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_cnx.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_parameter.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_parameter.c"


    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_command.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_command.c"

    
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_transaction.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_transaction.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_builder.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_builder.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite.rc"
)
 
add_library(libsqlite3 SHARED ${SRC_libsqlite3})
 
set_target_properties(libsqlite3 PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite.def")
 
set_target_properties(libsqlite3 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libsqlite3"
    SUFFIX ".dll"
    WINDOWS_EXPORT_ALL_SYMBOLS FALSE 
) 

target_compile_definitions(libsqlite3 PRIVATE
    WIN32 _WINDOWS _USRDLL UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS TARGET_OS_WIN32 WIN32_LEAN_AND_MEAN _WINSOCK_DEPRECATED_NO_WARNINGS
            NDEBUG
# ============================================================================
# CONFIGURACIÓN DE OPTIMIZACIÓN DE SQLITE (NÚCLEO)
# ============================================================================
        SQLITE_CORE=1
        SQLITE_THREADSAFE=1
        SQLITE_DEFAULT_FOREIGN_KEYS=1   
        SQLITE_OMIT_LOAD_EXTENSION  
        SQLITE_OMIT_DEPRECATED       
        SQLITE_OMIT_PROGRESS_CALLBACK 
        SQLITE_OMIT_SHARED_CACHE     
        SQLITE_OMIT_UTF16
        #SQLITE_OMIT_ANALYZE           # Descomentada: Ahorra ~10 KB
        #SQLITE_OMIT_VIRTUALTABLE      # Descomentada: ¡Ahorra de 80 KB a 100 KB!
        #SQLITE_OMIT_XFER_OPT          # Descomentada: Optimiza espacio de copia
        #SQLITE_OMIT_WAL 
) 


target_link_libraries(libsqlite3 PRIVATE winhttp crypt32 bcrypt ncrypt ${VFP_LIB})

set(DIR_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/${DIR_BUILD}")

set_target_properties(libsqlite3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DIR_BIN}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${DIR_BIN}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${DIR_BIN}/$<CONFIG>"
)
#    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../build_assets/lib/${DIR_BUILD}/$<CONFIG>"
 
add_custom_command(TARGET libsqlite3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/examples"
        "$<TARGET_FILE_DIR:libsqlite3>/"
)
