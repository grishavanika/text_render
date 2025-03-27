include_guard(GLOBAL)
include(CMakePrintHelpers)

find_package(Freetype REQUIRED)
#cmake_print_properties(TARGETS Freetype::Freetype
#    PROPERTIES
#    LOCATION INTERFACE_INCLUDE_DIRECTORIES)

add_library(freetype_Integrated INTERFACE)
target_link_libraries(freetype_Integrated INTERFACE Freetype::Freetype)
