include_guard(GLOBAL)
include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeFunctions.cmake)
add_library(ks_base
    KS_asserts.cc
    KS_asserts.hh
    KS_basic_math.hh
    )
CMAKE_setup_target(ks_base)
CMAKE_enable_warnings(ks_base)

target_include_directories(ks_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
