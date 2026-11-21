# Regression tests for libsqlite3 (run with CTest).
#
# The compat unit (internal helpers such as StrDetectEncoding) is compiled
# into the test binary because those helpers are intentionally not exported
# from the shared library.
add_executable(test_libsqlite3
    test_regression.c
    "${CMAKE_CURRENT_SOURCE_DIR}/../core/compat/sqlite_compat.c"
)
target_link_libraries(test_libsqlite3 PRIVATE libsqlite3)

if(MSVC)
    # The test sources contain UTF-8 literals
    target_compile_options(test_libsqlite3 PRIVATE /utf-8)
endif()

# Shared builds need the runtime library next to the test executable
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET test_libsqlite3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libsqlite3>
            $<TARGET_FILE_DIR:test_libsqlite3>)
endif()

enable_testing()
add_test(NAME regression COMMAND test_libsqlite3)

# API and .NET parity tests
add_executable(test_api test_api.c)
target_link_libraries(test_api PRIVATE libsqlite3)
if(MSVC)
    target_compile_options(test_api PRIVATE /utf-8)
endif()
if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET test_api POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libsqlite3>
            $<TARGET_FILE_DIR:test_api>)
endif()
add_test(NAME api COMMAND test_api)

# Export consistency: every public function must be resolvable in the
# shared library (keeps sqlite.def / visibility in sync with the headers).
add_executable(test_exports test_exports.c)
if(NOT WIN32)
    target_link_libraries(test_exports PRIVATE ${CMAKE_DL_LIBS})
endif()

if(BUILD_SHARED_LIBS)
    add_custom_command(TARGET test_exports POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:libsqlite3>
            $<TARGET_FILE_DIR:test_exports>)
    add_test(NAME exports COMMAND test_exports $<TARGET_FILE:libsqlite3>)
else()
    # Static builds have no dynamic exports to verify.
    add_test(NAME exports COMMAND ${CMAKE_COMMAND} -E echo "static build: no dynamic exports to verify")
endif()