include_guard(GLOBAL)

function(CMAKE_setup_target _target)
    target_compile_options(${_target} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    target_compile_options(${_target} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/MP>")
    ## target_compile_options(${_target} PRIVATE "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:DEBUG>>:/fsanitize=address>")
endfunction()

function(CMAKE_enable_warnings _target)
    set_property(TARGET ${_target} PROPERTY CXX_STANDARD 23)

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if (MSVC)
            target_compile_options(${_target} PUBLIC
                -Wall -Wextra -Werror)
        endif ()
    endif ()

    if (MSVC)
        target_compile_options(${_target} PUBLIC /W4 /WX)
        target_compile_options(${_target} PUBLIC /permissive-)

        # While correct, it's annoying during active development.
        # 4505 - unreferenced function with internal linkage has been removed
        target_compile_options(${_target} PUBLIC /wd4505)
    endif ()
endfunction()
