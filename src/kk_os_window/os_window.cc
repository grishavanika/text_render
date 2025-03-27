#if (_MSC_VER)
#  include <Windows.h>
#  include <dwmapi.h>
#endif
#include "KS_asserts.hh"
#include "os_window.hh"
#include "os_window_UTILS.hh"

#if (KK_WINDOW_OPENGL())
#include <glad/glad.h> // Replaces GL/GL.h
#endif
#if (KK_WINDOW_WIN32() && KK_WINDOW_OPENGL())
#  include <GL/glext.h>
#  include <GL/wglext.h>
#endif
#if (KK_WINDOW_GLFW() && KK_WINDOW_OPENGL())
#  include <GLFW/glfw3.h>
#endif
#if (KK_WINDOW_GLFW() && KK_WINDOW_VULKAN())
#  define GLFW_INCLUDE_NONE
#  define GLFW_INCLUDE_VULKAN
#  include <GLFW/glfw3.h>
#endif

#include <utility>

template<unsigned N>
static const char* DEBUG_GenerateTitle(char (&buffer)[N])
{
    static int wnd_count = 0;
    ++wnd_count;
#if defined(_MSC_VER)
    sprintf_s(buffer, N, "%s %i", KK_WINDOW_VERSION(), wnd_count);
#else
    sprintf(buffer, "%s %i", KK_WINDOW_VERSION(), wnd_count);
#endif
    return buffer;
}

OsWindow::OsWindow(OsWindow* main_window /*= nullptr*/)
    : context{}
    , user_data{}
{
    // #TODO: Should be in general `OsWindow_Init()`.
    // Also, hack with `::glfwInit()` needs to be moved to _Init() too.
    Wnd_SetProcessDPIAware();

    context = create_context(main_window);
    char title[256];
    Wnd_SetTitle(*this, DEBUG_GenerateTitle(title));
    Wnd_Show(*this);
}

OsWindow::~OsWindow()
{
    destroy();
}

#if (KK_WINDOW_WIN32())
static const wchar_t kWndClassName[] = L"{80E672FD-5515-48E2-88A9-3225713E2E5F}";

static LRESULT CALLBACK Impl_WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_NCCREATE)
    {
        const CREATESTRUCT* cs = reinterpret_cast<const CREATESTRUCT*>(lparam);
        KK_VERIFY(cs);
        if (!cs->lpCreateParams) // Fake window
            return ::DefWindowProc(hwnd, msg, wparam, lparam);
        OsWindow* self = static_cast<OsWindow*>(cs->lpCreateParams);
        KK_VERIFY(self);

        // ::SetWindowLongPtr().
        // To determine success or failure, clear the last error information by
        // calling SetLastError with 0, then call SetWindowLongPtr.
        // Function failure will be indicated by a return value
        // of zero and a GetLastError result that is nonzero.
        ::SetLastError(0);
        if ((::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self)) == 0)
            && (::GetLastError() != 0))
        {
            // Can't set the data. Fail ::CreateWindow() call.
            return FALSE;
        }
        return TRUE;
    }

    OsWindow* self = reinterpret_cast<OsWindow*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (self)
    {
        if (self->context.on_message)
            return self->context.on_message(*self, msg, wparam, lparam);
    }
    return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

#if (KK_WINDOW_OPENGL())
template<typename F>
static void OpenGL_Load(F& f, const char* f_name)
{
    PROC void_f = ::wglGetProcAddress(f_name);
    KK_VERIFY(void_f);
    f = reinterpret_cast<F>(void_f);
}

static OsWindowContext OpenGL_create_fake_context(HINSTANCE_ app_handle)
{
    OsWindowContext fake{};
    fake.hwnd = ::CreateWindowW(kWndClassName
        , L""
        , WS_CLIPSIBLINGS | WS_CLIPCHILDREN
        , 0 // x
        , 0 // y
        , 1 // w
        , 1 // h
        , nullptr
        , nullptr
        , (HINSTANCE)app_handle
        , nullptr);
    KK_VERIFY(fake.hwnd);

    fake.hdc = (HDC_)::GetDC((HWND)fake.hwnd);
    KK_VERIFY(fake.hdc);

    PIXELFORMATDESCRIPTOR fakePFD{};
    fakePFD.nSize = sizeof(fakePFD);
    fakePFD.nVersion = 1;
    fakePFD.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    fakePFD.iPixelType = PFD_TYPE_RGBA;
    fakePFD.cColorBits = 32;
    fakePFD.cAlphaBits = 8;
    fakePFD.cDepthBits = 24;

    int fakePFDID = ::ChoosePixelFormat((HDC)fake.hdc, &fakePFD);
    KK_VERIFY(fakePFDID);
    KK_VERIFY(::SetPixelFormat((HDC)fake.hdc, fakePFDID, &fakePFD));

    fake.hglrc = (HGLRC_)::wglCreateContext((HDC)fake.hdc);
    KK_VERIFY(fake.hglrc);
    return fake;
}
#endif

void OsWindow::destroy_context(const OsWindowContext& os_context)
{
#if (KK_WINDOW_OPENGL())
    KK_VERIFY(::wglDeleteContext((HGLRC)os_context.hglrc));
    (void)::ReleaseDC((HWND)os_context.hwnd, (HDC)os_context.hdc);
#endif
    (void)::DestroyWindow((HWND)os_context.hwnd);
}

OsWindowContext OsWindow::create_context(OsWindow* main_window)
{
#define KK_WND_ENABLE_TRANSPARENT() 0

    HINSTANCE app_handle = ::GetModuleHandle(nullptr);
    auto register_wnd_class = [&]
    {
        WNDCLASSEXW wcx{};
        wcx.cbSize = sizeof(wcx);
        wcx.style = CS_HREDRAW | CS_VREDRAW;
        wcx.lpfnWndProc = &Impl_WndProc;
        wcx.hInstance = app_handle;
        wcx.hIcon = ::LoadIcon(nullptr/*standard icon*/, IDI_APPLICATION);
        wcx.hCursor = ::LoadCursor(nullptr/*standard cursor*/, IDC_ARROW);
        wcx.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
        wcx.lpszClassName = kWndClassName;
        wcx.hIconSm = nullptr;

        ATOM class_atom = ::RegisterClassExW(&wcx);
        if (!class_atom && (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS))
            KK_VERIFY(false);
    };

    register_wnd_class();

#if (KK_WINDOW_OPENGL())
    OsWindowContext fake_context;
    HDC temp_HDC = nullptr;
    HGLRC temp_HGLRC = nullptr;
    if (main_window)
    {
        temp_HDC = (HDC)main_window->context.hdc;
        temp_HGLRC = (HGLRC)main_window->context.hglrc;
    }
    else
    {
        fake_context = OpenGL_create_fake_context((HINSTANCE_)app_handle);
        temp_HDC = (HDC)fake_context.hdc;
        temp_HGLRC = (HGLRC)fake_context.hglrc;
    }
    KK_VERIFY(temp_HDC);
    KK_VERIFY(temp_HGLRC);
    KK_VERIFY(::wglMakeCurrent(temp_HDC, temp_HGLRC));
    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
    OpenGL_Load(wglChoosePixelFormatARB, "wglChoosePixelFormatARB");
    OpenGL_Load(wglCreateContextAttribsARB, "wglCreateContextAttribsARB");
#else
    (void)main_window;
#endif

    OsWindowContext os_context;
    os_context.hwnd = ::CreateWindowW(
        kWndClassName,
        L"",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        nullptr, // no parent wnd
        nullptr, // no menu
        app_handle,
        this); // to be passed to WndProc
    KK_VERIFY(os_context.hwnd);
#if (KK_WINDOW_OPENGL())
    os_context.hdc = (HDC_)::GetDC((HWND)os_context.hwnd);
    KK_VERIFY(os_context.hdc);

    const int pixelAttribs[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_ALPHA_BITS_ARB, 8,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
        WGL_SAMPLES_ARB, 4,
#if (KK_WND_ENABLE_TRANSPARENT())
        WGL_TRANSPARENT_ARB, TRUE,
#endif
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        0, 0
    };
    int pixelFormatID = 0;
    UINT numFormats = 0;
    KK_VERIFY(wglChoosePixelFormatARB((HDC)os_context.hdc, pixelAttribs, nullptr, 1, &pixelFormatID, &numFormats));
    KK_VERIFY(numFormats > 0);

    PIXELFORMATDESCRIPTOR PFD{};
    KK_VERIFY(::DescribePixelFormat((HDC)os_context.hdc, pixelFormatID, sizeof(PFD), &PFD));
    KK_VERIFY(::SetPixelFormat((HDC)os_context.hdc, pixelFormatID, &PFD));

    const int  contextAttribs[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0, 0
    };

    HGLRC shared_context = (HGLRC)(main_window ? main_window->context.hglrc : nullptr);
    os_context.hglrc = wglCreateContextAttribsARB((HDC)os_context.hdc, shared_context, contextAttribs);
    KK_VERIFY(os_context.hglrc);

    KK_VERIFY(::wglMakeCurrent(nullptr, nullptr));
    if (fake_context.hwnd)
        destroy_context(fake_context);

    KK_VERIFY(::wglMakeCurrent((HDC)os_context.hdc, (HGLRC)os_context.hglrc));
    KK_VERIFY(::gladLoadGL());

#if (0)
    // Disable VSYNC.
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
    OpenGL_Load(wglSwapIntervalEXT, "wglSwapIntervalEXT");
    wglSwapIntervalEXT(0);
#endif
#endif

#if (KK_WND_ENABLE_TRANSPARENT())
    DWM_BLURBEHIND dwm_blur{};
    dwm_blur.dwFlags = DWM_BB_ENABLE | DWM_BB_TRANSITIONONMAXIMIZED;
    dwm_blur.fEnable = TRUE;
    dwm_blur.hRgnBlur = nullptr;
    dwm_blur.fTransitionOnMaximized = TRUE;
    KK_VERIFY(::DwmEnableBlurBehindWindow((HWND)os_context.hwnd, &dwm_blur) == S_OK);
#endif
    return os_context;
}

void OsWindow::destroy()
{
    if (!context.hwnd)
        return;

    const OsWindow* self = reinterpret_cast<const OsWindow*>(
        ::SetWindowLongPtr((HWND)context.hwnd, GWLP_USERDATA, LONG_PTR(0)));
    KK_VERIFY(self == this || !::IsWindow((HWND)context.hwnd));
    destroy_context(context);

#if (0) // We don't want to unregister class since we actually don't know
        // if we are the last client/window (user of this class).
    HINSTANCE app_handle = ::GetModuleHandle(nullptr);
    KK_VERIFY(::UnregisterClassW(kWndClassName, app_handle));
#endif

    context = {};
}
#endif

#if (KK_WINDOW_GLFW())

static int& GLFW_InitCount()
{
    static int count = 0;
    return count;
}

static void GLFW_TryInit()
{
    if (++GLFW_InitCount() == 1)
        KK_VERIFY(::glfwInit());
}

static void GLFW_TryDeinit()
{
    if (--GLFW_InitCount() == 0)
        ::glfwTerminate();
}

OsWindowContext OsWindow::create_context(OsWindow* main_window)
{
    OsWindowContext os_context;

    GLFW_TryInit();

    ::glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

#if (KK_WINDOW_VULKAN())
    ::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* shared_context = nullptr;
    (void)main_window;
#elif (KK_WINDOW_OPENGL())
    ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    ::glfwWindowHint(GLFW_SAMPLES, 4); // MSAA.
    ::glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
    GLFWwindow* shared_context = (main_window ? main_window->context.glfw_wnd : nullptr);
#endif

    os_context.glfw_wnd = ::glfwCreateWindow(800, 600, "", nullptr, shared_context);
    KK_VERIFY(os_context.glfw_wnd);
    glfwSetWindowUserPointer(os_context.glfw_wnd, this);

#if (KK_WINDOW_OPENGL())
    ::glfwMakeContextCurrent(os_context.glfw_wnd);
    KK_VERIFY(gladLoadGL());
#endif
    return os_context;
}

void OsWindow::destroy_context(const OsWindowContext& os_context)
{
    KK_VERIFY(os_context.glfw_wnd);
    KK_VERIFY(glfwGetWindowUserPointer(os_context.glfw_wnd) == this);
    glfwSetWindowUserPointer(os_context.glfw_wnd, nullptr);
    ::glfwDestroyWindow(os_context.glfw_wnd);
    GLFW_TryDeinit();
}

void OsWindow::destroy()
{
    if (!context.glfw_wnd)
        return;
    destroy_context(context);
    context = {};
}
#endif

