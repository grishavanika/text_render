cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

set(VCPKG_INSTALLED_DIR "${CMAKE_SOURCE_DIR}/build_vcpkg_installed")

option(KK_BUILD_RENDER_OPENGL "OpenGL as a backend"               ON)
option(KK_BUILD_RENDER_VULKAN "Vulkan as a backend"               OFF)
option(KK_BUILD_WND_GLFW      "Use GLFW for OsWindow"             ON)
option(KK_BUILD_WND_WIN32     "Use native Win32 API for OsWindow" OFF)

message("OpenGL: ${KK_BUILD_RENDER_OPENGL}")
message("Vulkan: ${KK_BUILD_RENDER_VULKAN}")
message("Window (WIN32): ${KK_BUILD_WND_WIN32}")
message("Window (GLFW): ${KK_BUILD_WND_GLFW}")

if (KK_BUILD_RENDER_OPENGL)
  set(SLN_NAME "OpenGL")
elseif (KK_BUILD_RENDER_VULKAN)
  set(SLN_NAME "Vulkan")
endif()
if (KK_BUILD_WND_WIN32)
  set(SLN_NAME "${SLN_NAME}_Win32")
elseif (KK_BUILD_WND_GLFW)
  set(SLN_NAME "${SLN_NAME}_GLFW")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(SLN_NAME "CLANG_${SLN_NAME}")
endif ()

project("KK_${SLN_NAME}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(src)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT "test_HWND")
