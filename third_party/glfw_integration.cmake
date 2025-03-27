include_guard(GLOBAL)
include(CMakePrintHelpers)

find_package(glfw3 CONFIG REQUIRED)
#cmake_print_properties(TARGETS glfw
#    PROPERTIES
#    LOCATION INTERFACE_INCLUDE_DIRECTORIES)

add_library(glfw_Integrated INTERFACE)
target_link_libraries(glfw_Integrated INTERFACE glfw)
