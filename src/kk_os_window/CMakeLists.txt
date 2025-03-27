include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeFunctions.cmake)

if (KK_BUILD_RENDER_OPENGL)
  include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/glad_integration.cmake)
endif ()
if (KK_BUILD_RENDER_VULKAN)
  include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/vulkan_integration.cmake)
endif ()
if (KK_BUILD_WND_GLFW)
  include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/glfw_integration.cmake)
endif ()

add_library(kk_os_window
    os_window_config.hh
    os_window.hh os_window.cc
    os_window_UTILS.cc os_window_UTILS.hh
    )
CMAKE_setup_target(kk_os_window)
CMAKE_enable_warnings(kk_os_window)

target_link_libraries(kk_os_window PUBLIC ks_base)

if (MSVC)
    target_link_libraries(kk_os_window PUBLIC Dwmapi)
endif ()

if (KK_BUILD_RENDER_OPENGL)
    target_compile_definitions(kk_os_window PUBLIC KK_BUILD_RENDER_OPENGL=1)
endif ()
if (KK_BUILD_RENDER_VULKAN)
    target_compile_definitions(kk_os_window PUBLIC KK_BUILD_RENDER_VULKAN=1)
endif ()

if (KK_BUILD_WND_WIN32 AND KK_BUILD_RENDER_OPENGL)
    if (MSVC)
        target_link_libraries(kk_os_window PUBLIC opengl32 glad_Integrated)
    endif ()
endif ()

if (KK_BUILD_WND_GLFW)
    target_compile_definitions(kk_os_window PUBLIC KK_BUILD_WND_GLFW=1)
    target_link_libraries(kk_os_window PUBLIC glfw_Integrated)
    if (KK_BUILD_RENDER_VULKAN)
        target_link_libraries(kk_os_window PUBLIC vulkan_Integrated)
    endif ()
endif ()
if (KK_BUILD_WND_WIN32)
    target_compile_definitions(kk_os_window PUBLIC KK_BUILD_WND_WIN32=1)
endif ()

target_include_directories(kk_os_window PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# To make WM_CHAR work properly, even tho we use ::RegisterClassExW().
target_compile_definitions(kk_os_window PRIVATE UNICODE _UNICODE)
