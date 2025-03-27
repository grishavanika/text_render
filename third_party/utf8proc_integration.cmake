include_guard(GLOBAL)
add_library(utf8proc_Integrated INTERFACE)
find_package(unofficial-utf8proc CONFIG REQUIRED)
target_link_libraries(utf8proc_Integrated INTERFACE utf8proc)
