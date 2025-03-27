include(${CMAKE_CURRENT_LIST_DIR}/../../CMakeFunctions.cmake)

add_executable(test_HWND main.cc)
CMAKE_setup_target(test_HWND)
CMAKE_enable_warnings(test_HWND)

target_link_libraries(test_HWND kr_render)
target_link_libraries(test_HWND kk_os_render)
