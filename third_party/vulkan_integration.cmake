include_guard(GLOBAL)
include(CMakePrintHelpers)

# See $Env:VULKAN_SDK
include(FindVulkan)
if (NOT Vulkan_FOUND)
    message(FATAL_ERROR "Failed to find Vulkan. Is $Env:VULKAN_SDK set?")
endif ()
#cmake_print_variables(Vulkan_INCLUDE_DIR)
#cmake_print_variables(Vulkan_LIBRARY)

add_library(vulkan_Integrated INTERFACE)
target_include_directories(vulkan_Integrated INTERFACE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vulkan_Integrated INTERFACE ${Vulkan_LIBRARY})

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if (MSVC)
        target_compile_options(vulkan_Integrated INTERFACE
            -Wno-old-style-cast
            )
    endif ()
endif ()
