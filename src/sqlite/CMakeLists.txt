cmake_minimum_required(VERSION 3.15)
project(libsqlite3 LANGUAGES C)

# ============================================================================
# Version: single source of truth is core/sqlite_core.h
# ============================================================================
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core.h" _v_major REGEX "^#define SQLITE_CORE_VERSION_MAJOR")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core.h" _v_minor REGEX "^#define SQLITE_CORE_VERSION_MINOR")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core.h" _v_patch REGEX "^#define SQLITE_CORE_VERSION_PATCH")

string(REGEX REPLACE "^#define SQLITE_CORE_VERSION_MAJOR +([0-9]+).*" "\\1" SQLITE_CORE_VERSION_MAJOR "${_v_major}")
string(REGEX REPLACE "^#define SQLITE_CORE_VERSION_MINOR +([0-9]+).*" "\\1" SQLITE_CORE_VERSION_MINOR "${_v_minor}")
string(REGEX REPLACE "^#define SQLITE_CORE_VERSION_PATCH +([0-9]+).*" "\\1" SQLITE_CORE_VERSION_PATCH "${_v_patch}")

set(SQLITE_CORE_VERSION "${SQLITE_CORE_VERSION_MAJOR}.${SQLITE_CORE_VERSION_MINOR}.${SQLITE_CORE_VERSION_PATCH}")
set(SQLITE_CORE_RC_FILEVERSION "${SQLITE_CORE_VERSION_MAJOR},${SQLITE_CORE_VERSION_MINOR},${SQLITE_CORE_VERSION_PATCH},0")
set(SQLITE_CORE_RC_PRODUCTVERSION "${SQLITE_CORE_RC_FILEVERSION}")
set(SQLITE_CORE_RC_FILEVERSION_STR "${SQLITE_CORE_VERSION}.0")
set(SQLITE_CORE_RC_PRODUCTVERSION_STR "${SQLITE_CORE_VERSION}.0")
set(SQLITE_CORE_COMPANY "libsqlite3 contributors")
set(SQLITE_CORE_COPYRIGHT "(C) 2026. Christian Gutman All rights reserved.")

message(STATUS "libsqlite3 version: ${SQLITE_CORE_VERSION} (from core/sqlite_core.h)")

# Windows resource file is generated so the version always matches the header.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sqlite.rc"
    @ONLY
)

# ============================================================================
# Language standard and build defaults
# ============================================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configurations" FORCE)
endif()

option(BUILD_SHARED_LIBS "Build libsqlite3 as a shared library (DLL/.so/.dylib)" ON)
option(LIBSQLITE3_BUILD_TESTS "Build the regression tests (CTest)" ON)

# ============================================================================
# Output layout: <repo>/bin/<arch>-<os>/<config>/
# ============================================================================
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(LIBSQLITE_ARCH_DIR "x32")
else()
    set(LIBSQLITE_ARCH_DIR "x64")
endif()
if(WIN32)
    set(LIBSQLITE_SYS_DIR "Windows")
elseif(APPLE)
    set(LIBSQLITE_SYS_DIR "Darwin")
else()
    set(LIBSQLITE_SYS_DIR "Linux")
endif()

# Shared and static artifacts go to distinct folders (both produce
# libsqlite3.lib on Windows, so they must not collide).
if(BUILD_SHARED_LIBS)
    set(LIBSQLITE_KIND_DIR "shared")
else()
    set(LIBSQLITE_KIND_DIR "static")
endif()

set(LIBSQLITE_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../bin/${LIBSQLITE_ARCH_DIR}-${LIBSQLITE_SYS_DIR}/${LIBSQLITE_KIND_DIR}")

# ============================================================================
# Sources
# ============================================================================
set(SRC_libsqlite3
    # Absorbed portability layer (former external dependencies)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/compat/sqlite_compat.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/compat/sqlite_compat.c"

    # Embedded engine (upstream sqlite3.c amalgamation, unmodified)
    "${CMAKE_CURRENT_SOURCE_DIR}/core/engine/sqlite3.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/engine/sqlite3.c"

    # Public API and modules
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

    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_field.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite_core_field.c"
)

if(WIN32)
    list(APPEND SRC_libsqlite3 "${CMAKE_CURRENT_BINARY_DIR}/sqlite.rc")
    # Windows exports are controlled by core/sqlite.def; pass it as a linker
    # input only for shared builds (a .def makes no sense for static libs).
    if(BUILD_SHARED_LIBS)
        list(APPEND SRC_libsqlite3 "${CMAKE_CURRENT_SOURCE_DIR}/core/sqlite.def")
    endif()
endif()

# ============================================================================
# Target
# ============================================================================
add_library(libsqlite3 ${SRC_libsqlite3})

target_include_directories(libsqlite3 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>          # sqlite_core.h and modules
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core/engine>   # sqlite3.h
)
target_include_directories(libsqlite3 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/core/compat"     # absorbed portability layer
)

# Engine configuration (portable across platforms)
target_compile_definitions(libsqlite3 PRIVATE
    SQLITE_CORE=1
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_FOREIGN_KEYS=1
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_OMIT_UTF16
)

if(MSVC)
    target_compile_definitions(libsqlite3 PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    target_compile_options(libsqlite3 PRIVATE /W4)
endif()

if(WIN32)
    target_link_libraries(libsqlite3 PRIVATE winhttp crypt32 bcrypt ncrypt)
endif()

# PREFIX "" avoids the doubled "liblibsqlite3.so" on Unix.
# Default suffixes are correct per library type: .dll/.so/.dylib (shared),
# .lib (static, Windows).
set_target_properties(libsqlite3 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libsqlite3"
)

set_target_properties(libsqlite3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${LIBSQLITE_BIN_DIR}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${LIBSQLITE_BIN_DIR}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${LIBSQLITE_BIN_DIR}/$<CONFIG>"
)

# ============================================================================
# Install and CMake package
# ============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS libsqlite3
    EXPORT libsqlite3Targets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Public headers: the whole core/ tree (public modules + engine + compat)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/core/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libsqlite3"
    FILES_MATCHING
    PATTERN "*.h"
)

set(LIBSQLITE3_CMAKE_CONFIG_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/libsqlite3")

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/libsqlite3Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/libsqlite3Config.cmake"
    INSTALL_DESTINATION "${LIBSQLITE3_CMAKE_CONFIG_DESTINATION}"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/libsqlite3ConfigVersion.cmake"
    VERSION "${SQLITE_CORE_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/libsqlite3Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/libsqlite3ConfigVersion.cmake"
    DESTINATION "${LIBSQLITE3_CMAKE_CONFIG_DESTINATION}"
)

install(EXPORT libsqlite3Targets
    NAMESPACE libsqlite3::
    DESTINATION "${LIBSQLITE3_CMAKE_CONFIG_DESTINATION}"
)

# ============================================================================
# Examples are copied next to the binaries (post-build convenience)
# ============================================================================
add_custom_command(TARGET libsqlite3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/examples"
        "$<TARGET_FILE_DIR:libsqlite3>/"
)

# ============================================================================
# Tests
# ============================================================================
if(LIBSQLITE3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()