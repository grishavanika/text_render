#pragma once

#if defined(KK_BUILD_RENDER_OPENGL) && (KK_BUILD_RENDER_OPENGL == 1)
#  define KK_WINDOW_OPENGL() 1
#else
#  define KK_WINDOW_OPENGL() 0
#endif
#if defined(KK_BUILD_RENDER_VULKAN) && (KK_BUILD_RENDER_VULKAN == 1)
#  define KK_WINDOW_VULKAN() 1
#else
#  define KK_WINDOW_VULKAN() 0
#endif

static_assert(0
    + int(KK_WINDOW_OPENGL())
    + int(KK_WINDOW_VULKAN())
    == 1
    , "KK_WINDOW_*: only one backend should be selected.");

#if defined(KK_BUILD_WND_WIN32) && (KK_BUILD_WND_WIN32 == 1)
#  define KK_WINDOW_WIN32() 1
#else
#  define KK_WINDOW_WIN32() 0
#endif
#if defined(KK_BUILD_WND_GLFW) && (KK_BUILD_WND_GLFW == 1)
#  define KK_WINDOW_GLFW() 1
#else
#  define KK_WINDOW_GLFW() 0
#endif

static_assert(0
    + int(KK_WINDOW_WIN32())
    + int(KK_WINDOW_GLFW())
    == 1
    , "KK_WINDOW_WIN32/GLFW*: only one Window backend should be selected.");

class OsWindow;

#if (KK_WINDOW_WIN32())
#include <functional>

// To avoid Windows.h in the header.
using HWND_ = void*;
using HDC_ = void*;
using HGLRC_ = void*;
using LRESULT_ = std::int64_t;
using UINT_ = unsigned int;
using WPARAM_ = std::uint64_t;
using LPARAM_ = std::int64_t;
using HINSTANCE_ = std::int64_t;
using OnWin32Message = std::function<LRESULT_ (OsWindow& wnd, UINT_ msg, WPARAM_ wparam, LPARAM_ lparam)>;

// Vulkan/OpenGL.
struct OsWindowContext_Win32
{
    OnWin32Message on_message;
    HWND_ hwnd{};
#if (KK_WINDOW_OPENGL())
    HDC_ hdc{};
    HGLRC_ hglrc{};
#endif
};

using OsWindowContext = OsWindowContext_Win32;
#endif

#if (KK_WINDOW_GLFW())
struct GLFWwindow;
// Vulkan/OpenGL.
struct OsWindowContext_GLFW
{
    void* glfw_wnd_callback = nullptr;
    GLFWwindow* glfw_wnd = nullptr;
};

using OsWindowContext = OsWindowContext_GLFW;
#endif

#if (KK_WINDOW_GLFW() && KK_WINDOW_OPENGL())
#  define KK_WINDOW_VERSION() "OpenGL (GLFW)"
#endif
#if (KK_WINDOW_GLFW() && KK_WINDOW_VULKAN())
#  define KK_WINDOW_VERSION() "Vulkan (GLFW)"
#endif
#if (KK_WINDOW_WIN32() && KK_WINDOW_OPENGL())
#  define KK_WINDOW_VERSION() "OpenGL (Win32)"
#endif
#if (KK_WINDOW_WIN32() && KK_WINDOW_VULKAN())
#  define KK_WINDOW_VERSION() "Vulkan (Win32)"
#endif
