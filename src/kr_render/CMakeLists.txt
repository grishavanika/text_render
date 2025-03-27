include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeFunctions.cmake)
include(CMakePrintHelpers)
include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/freetype_integration.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/utf8proc_integration.cmake)

if (KK_BUILD_RENDER_OPENGL)
  include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/glad_integration.cmake)
endif ()
if (KK_BUILD_RENDER_VULKAN)
  include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/vulkan_integration.cmake)
endif ()

add_library(kr_render
    KR_kids_api_fwd.hh
    KR_kids_config.hh
    KR_kids_font.cc
    KR_kids_font.hh
    KR_kids_font_fallback.cc
    KR_kids_font_fallback.hh
    KR_kids_image.cc
    KR_kids_image.hh
    KR_kids_render.cc
    KR_kids_render.hh
    KR_kids_UTF8_text.hh
    KR_render_utils.hh
    KR_render_utils.cc
    KR_text_shaper.cc
    KR_text_shaper.hh
    )
CMAKE_setup_target(kr_render)
CMAKE_enable_warnings(kr_render)

if (KK_BUILD_RENDER_OPENGL)
    target_compile_definitions(kr_render PUBLIC KK_BUILD_RENDER_OPENGL=1)
    target_link_libraries(kr_render PUBLIC glad_Integrated)
endif ()
if (KK_BUILD_RENDER_VULKAN)
    target_compile_definitions(kr_render PUBLIC KK_BUILD_RENDER_VULKAN=1)
    target_link_libraries(kr_render PUBLIC vulkan_Integrated)
endif ()

target_link_libraries(kr_render PUBLIC ks_base)
target_link_libraries(kr_render PUBLIC freetype_Integrated)
target_link_libraries(kr_render PUBLIC utf8proc_Integrated)

target_include_directories(kr_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
