include_guard(GLOBAL)
include(CMakePrintHelpers)

find_package(glad CONFIG REQUIRED)
#cmake_print_properties(TARGETS glad::glad
#    PROPERTIES
#    LOCATION INTERFACE_INCLUDE_DIRECTORIES)

add_library(glad_Integrated INTERFACE)
target_link_libraries(glad_Integrated INTERFACE glad::glad)
