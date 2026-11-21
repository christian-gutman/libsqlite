# Usage examples. Each one is an tiny, self-contained program that links
# the public library exactly like a consumer would, validates every call
# and exits with the number of failed checks — so they double as smoke
# tests and are registered in CTest.

foreach(_example basic parameters transaction json)
    add_executable(example_${_example} ${_example}.c)
    target_link_libraries(example_${_example} PRIVATE libsqlite3)

    if(MSVC)
        # The sources contain UTF-8 literals ("Café"): interpret them as such.
        target_compile_options(example_${_example} PRIVATE /utf-8)
    endif()

    if(BUILD_SHARED_LIBS)
        add_custom_command(TARGET example_${_example} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:libsqlite3>
                $<TARGET_FILE_DIR:example_${_example}>)
    endif()

    add_test(NAME example_${_example} COMMAND example_${_example})
endforeach()