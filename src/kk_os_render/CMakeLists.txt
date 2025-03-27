include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeFunctions.cmake)

SET(render_FILES os_render_vulkan.cc os_render_opengl.cc)
set_source_files_properties(${render_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
if (KK_BUILD_RENDER_OPENGL)
    set_source_files_properties(os_render_opengl.cc PROPERTIES HEADER_FILE_ONLY FALSE)
endif ()
if (KK_BUILD_RENDER_VULKAN)
    set_source_files_properties(os_render_vulkan.cc PROPERTIES HEADER_FILE_ONLY FALSE)
endif ()

add_library(kk_os_render ${render_FILES} os_render_backend.hh)
CMAKE_setup_target(kk_os_render)
CMAKE_enable_warnings(kk_os_render)

target_link_libraries(kk_os_render kr_render kk_os_window)
if (KK_BUILD_RENDER_OPENGL)
    if(MSVC)
        target_link_libraries(kk_os_render opengl32)
    endif()
endif ()
if(MSVC)
    target_link_libraries(kk_os_render Dwmapi)
endif()

target_include_directories(kk_os_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# To make WM_CHAR work properly, even tho we use ::RegisterClassExW().
target_compile_definitions(kk_os_render PRIVATE UNICODE _UNICODE)
