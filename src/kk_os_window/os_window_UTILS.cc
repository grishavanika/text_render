#if (_MSC_VER)
#include <Windows.h>
#include <windowsx.h>
#include <commdlg.h>
#endif

#include "KS_asserts.hh"
#include "os_window.hh"
#include "os_window_UTILS.hh"

#if (KK_WINDOW_GLFW())
#  include <GLFW/glfw3.h>
#  if defined(_MSC_VER) // ::glfwGetWin32Window()
#    define GLFW_EXPOSE_NATIVE_WIN32
#    include <GLFW/glfw3native.h>
#  endif
#endif

#include <cmath>

/*virtual*/ void Wnd_EventCallbacks::OnMouseMove(OsWindow& /*wnd*/, int /*cursor_x*/, int /*cursor_y*/, Wnd_MouseButtons /*buttons*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnMouseVScroll(OsWindow& /*wnd*/, float /*delta*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnMouseHScroll(OsWindow& /*wnd*/, float /*delta*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnMouseButton(OsWindow& /*wnd*/, Wnd_MouseButtons /*buttons*/, Wnd_ButtonAction /*action*/, Wnd_Modifiers /*modifiers*/, int /*mouse_x*/, int /*mouse_y*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnKeyboard(OsWindow& /*wnd*/, Wnd_VirtualKey /*virtual_key*/, Wnd_ButtonAction /*action*/, Wnd_Modifiers /*modifiers*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnInputCharacter(OsWindow& /*wnd*/, std::uint32_t /*codepoint*/, Wnd_Modifiers /*modifiers*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnFocus(OsWindow& /*wnd*/, bool /*has_focus*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnWndMove(OsWindow& /*wnd*/, int /*x*/, int /*y*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnWndResize(OsWindow& /*wnd*/, int /*width*/, int /*height*/) {}
/*virtual*/ void Wnd_EventCallbacks::OnWndDPIChanged(OsWindow& /*wnd*/) {}

#if (_MSC_VER)
static HWND Hwnd(OsWindow& wnd)
{
#if (KK_WINDOW_WIN32())
    return (HWND)wnd.context.hwnd;
#elif (KK_WINDOW_GLFW())
    return ::glfwGetWin32Window(wnd.context.glfw_wnd);
#else 
#  error Unknown Window backend.
#endif
}
#endif

std::string Wnd_OpenFileDialog_Blocking(OsWindow& wnd)
{
#if (_MSC_VER)
    // https://docs.microsoft.com/en-us/windows/win32/dlgbox/using-common-dialog-boxes?redirectedfrom=MSDN#open_file
    char szFile[260]{};

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = Hwnd(wnd);
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = nullptr;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    const BOOL opened = ::GetOpenFileNameA(&ofn);
    if (!opened)
        return std::string();
    return std::string(szFile);
#else
    (void)wnd;
    return std::string();
#endif
}

#if (KK_WINDOW_WIN32())
void Wnd_SetProcessDPIAware()
{
#if (0) // To handle all Windows version.
    if (_glfwIsWindows10Version1703OrGreaterWin32())
        ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else if (IsWindows8Point1OrGreater())
        ::SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    else if (IsWindowsVistaOrGreater())
        ::SetProcessDPIAware();
#else
    const BOOL ok = ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const DWORD error = ::GetLastError();
    // [...] ERROR_ACCESS_DENIED if the default API awareness mode for the process has already been set.
    KK_VERIFY(ok || (error == ERROR_ACCESS_DENIED));
#endif
}

#if (0) // https://github.com/tringi/win32-dpi/blob/4aa07f61d0c573d80797ddf004ef6ee75ea77829/win32-dpi.cpp#L46-L60
UINT GetDPI(HWND hWnd) {
    if (hWnd != NULL) {
        if (pfnGetDpiForWindow)
            return pfnGetDpiForWindow(hWnd);
    }
    else {
        if (pfnGetDpiForSystem)
            return pfnGetDpiForSystem();
    }
    if (HDC hDC = GetDC(hWnd)) {
        auto dpi = GetDeviceCaps(hDC, LOGPIXELSX);
        ReleaseDC(hWnd, hDC);
        return dpi;
    }
    else
        return USER_DEFAULT_SCREEN_DPI;
}
#endif

unsigned Wnd_GetDPI(OsWindow& wnd)
{
#if (!KK_DISABLE_DPI())
    const UINT dpi = ::GetDpiForWindow(Hwnd(wnd));
    KK_VERIFY(dpi != 0);
    return unsigned(dpi);
#else
    (void)wnd;
    return USER_DEFAULT_SCREEN_DPI;
#endif
}

kk::Vec2f Wnd_GetDPIScale(OsWindow& wnd)
{
    const unsigned dpi = Wnd_GetDPI(wnd);
    const unsigned default_dpi = USER_DEFAULT_SCREEN_DPI; // 96.
    const float scale = float(dpi) / default_dpi;
    return {scale, scale};
}

static DWORD Wnd_GetStyle_(OsWindow& wnd)
{
    const LONG v = ::GetWindowLongW(Hwnd(wnd), GWL_STYLE);
    // KK_VERIFY(v);
    return DWORD(v);
}

static DWORD Wnd_GetStyleEx_(OsWindow& wnd)
{
    const LONG v = ::GetWindowLongW(Hwnd(wnd), GWL_EXSTYLE);
    // KK_VERIFY(v);
    return DWORD(v);
}

static Wnd_Modifiers Wnd_GetKeyModifiers()
{
    unsigned mods = Wnd_Modifiers::None;
    if (::GetKeyState(VK_SHIFT) & 0x8000)
        mods |= Wnd_Modifiers::Shift;
    if (::GetKeyState(VK_CONTROL) & 0x8000)
        mods |= Wnd_Modifiers::Ctrl;
    if (::GetKeyState(VK_MENU) & 0x8000)
        mods |= Wnd_Modifiers::Alt;
    if ((::GetKeyState(VK_LWIN) | ::GetKeyState(VK_RWIN)) & 0x8000)
        mods |= Wnd_Modifiers::Win;
    if (::GetKeyState(VK_CAPITAL) & 1)
        mods |= Wnd_Modifiers::CapsLock;
    if (::GetKeyState(VK_NUMLOCK) & 1)
        mods |= Wnd_Modifiers::NumLock;
    return Wnd_Modifiers{mods};
}

void Wnd_AllEventsPoll()
{
    MSG msg{};
    while (::PeekMessageW(&msg, nullptr/*current thread windows*/, 0, 0, PM_REMOVE))
    {
        (void)::TranslateMessage(&msg);
        (void)::DispatchMessageW(&msg);
    }
}

kk::Size Wnd_Size(OsWindow& wnd)
{
    RECT r{};
    (void)::GetClientRect(Hwnd(wnd), &r);
    kk::Size size{};
    size.width = (r.right - r.left);
    size.height = (r.bottom - r.top);
    return size;
}

struct Wnd_ProcState
{
    int mouse_captured_count_ = 0;
    std::uint32_t high_surrogateUTF16 = 0;
};

static std::uint32_t Wnd_CodepointFrom_WM_CHAR(OsWindow& wnd, Wnd_ProcState& state, /*WPARAM*/std::uint64_t wparam)
{
    KK_VERIFY(::IsWindowUnicode(Hwnd(wnd)));
#if !defined(UNICODE) || !defined(_UNICODE)
#  error Proper handling of WM_CHAR requires UNICODE,_UNICODE defined
#endif
    if (wparam >= 0xd800 && wparam <= 0xdbff)
    {
        state.high_surrogateUTF16 = std::uint32_t(WCHAR(wparam));
        return std::uint32_t(-1);
    }

    std::uint32_t codepoint = 0;
    if (wparam >= 0xdc00 && wparam <= 0xdfff)
    {
        if (state.high_surrogateUTF16)
        {
            codepoint += (state.high_surrogateUTF16 - 0xd800) << 10;
            codepoint += std::uint32_t(WCHAR(wparam)) - 0xdc00;
            codepoint += 0x10000;
        }
    }
    else
        codepoint = std::uint32_t(WCHAR(wparam));

    state.high_surrogateUTF16 = 0;
    return codepoint;
}

void Wnd_Show(OsWindow& wnd)
{
    (void)::ShowWindow(Hwnd(wnd), SW_SHOW);
}

void Wnd_Hide(OsWindow& wnd)
{
    (void)::ShowWindow(Hwnd(wnd), SW_HIDE);
}

void Wnd_SetTitle(OsWindow& wnd, const char* title)
{
    // #TODO: expect UTF8, use *W().
    KK_VERIFY(::SetWindowTextA(Hwnd(wnd), title));
}

bool Wnd_IsAlive(OsWindow& wnd)
{
    return !!::IsWindow(Hwnd(wnd));
}

void Wnd_SetClipboardTextUTF8(OsWindow& wnd, const std::string& text_utf8)
{
    struct Scope
    {
        HANDLE object = nullptr;
        WCHAR* buffer = nullptr;

        ~Scope()
        {
            if (object)
                ::GlobalFree(object);
        }
    } data;

    int ch_count = ::MultiByteToWideChar(CP_UTF8, 0, text_utf8.c_str(), -1, nullptr, 0);
    if (!ch_count)
        return;

    data.object = ::GlobalAlloc(GMEM_MOVEABLE, ch_count * sizeof(WCHAR));
    if (!data.object)
        return;
    data.buffer = static_cast<WCHAR*>(::GlobalLock(data.object));
    if (!data.buffer)
        return;

    if (!::MultiByteToWideChar(CP_UTF8, 0, text_utf8.c_str(), -1, data.buffer, ch_count))
        return;
    ::GlobalUnlock(data.object);

    if (!::OpenClipboard(Hwnd(wnd)))
        return;

    (void)::EmptyClipboard();
    (void)::SetClipboardData(CF_UNICODETEXT, data.object);
    (void)::CloseClipboard();
}

std::string Wnd_GetClipboardTextUTF8(OsWindow& wnd)
{
    struct Scope
    {
        bool opened_ = false;
        HANDLE object = nullptr;
        WCHAR* buffer = nullptr;

        ~Scope()
        {
            if (opened_)
                ::CloseClipboard();
            if (object)
                ::GlobalUnlock(object);
        }
    } data;

    data.opened_ = !!::OpenClipboard(Hwnd(wnd));
    if (!data.opened_)
        return std::string();
    HANDLE object = ::GetClipboardData(CF_UNICODETEXT);
    if (!object)
        return std::string();
    data.buffer = static_cast<WCHAR*>(::GlobalLock(object));
    if (!data.buffer)
        return std::string();
    data.object = object;

    int size = ::WideCharToMultiByte(CP_UTF8, 0, data.buffer, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return std::string();
    
    std::string str;
    str.resize(std::size_t(size));
    if (!::WideCharToMultiByte(CP_UTF8, 0, data.buffer, -1, &str[0], size, nullptr, nullptr))
        return std::string();

    return str;
}

OsCursor* Wnd_LoadStandardCursor(Wnd_StandardCursor cursor_type)
{
    const LPWSTR id = [&cursor_type]()
    {
        switch (cursor_type)
        {
        case Wnd_StandardCursor::Standard: return IDC_ARROW;
        case Wnd_StandardCursor::Hand: return IDC_HAND;
        case Wnd_StandardCursor::Size_DiagonalMain: return IDC_SIZENWSE;
        case Wnd_StandardCursor::Size_DiagonalSecondary: return IDC_SIZENESW;
        case Wnd_StandardCursor::Size_Vertical: return IDC_SIZENS;
        case Wnd_StandardCursor::Size_Horizontal: return IDC_SIZEWE;
        }
        return IDC_ARROW;
    }();
    HCURSOR cursor = ::LoadCursorW(nullptr, id);
    KK_VERIFY(cursor);
    return (OsCursor*)cursor;
}

void Wnd_SetCursor(OsWindow& wnd, OsCursor* cursor /*= nullptr*/)
{
    if (!cursor)
        cursor = Wnd_LoadStandardCursor(Wnd_StandardCursor::Standard);
    HCURSOR impl = (HCURSOR)cursor;
    KK_VERIFY(::SetClassLongPtrW(Hwnd(wnd), GCLP_HCURSOR, (LONG_PTR)impl));
}

kk::Point Wnd_GetMousePosition(OsWindow& wnd)
{
    POINT p{};
    (void)::GetCursorPos(&p);
    (void)::ScreenToClient(Hwnd(wnd), &p);
    return {p.x, p.y};
}

kk::Point Wnd_GetPosition(OsWindow& wnd)
{
    POINT p{};
    (void)::ClientToScreen(Hwnd(wnd), &p);
    return {p.x, p.y};
}

void Wnd_SetPosition(OsWindow& wnd, int x, int y)
{
    RECT r{x, y, x, y};
    KK_VERIFY(::AdjustWindowRectExForDpi(&r
        , Wnd_GetStyle_(wnd)
        , FALSE // no menu
        , Wnd_GetStyleEx_(wnd)
        , Wnd_GetDPI(wnd)));
    KK_VERIFY(::SetWindowPos(Hwnd(wnd)
        , nullptr
        , r.left
        , r.top
        , 0
        , 0
        , SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE));
}

void Wnd_SetSize(OsWindow& wnd, int width, int height)
{
    RECT r{0, 0, width, height};
    KK_VERIFY(::AdjustWindowRectExForDpi(&r
        , Wnd_GetStyle_(wnd)
        , FALSE // no menu
        , Wnd_GetStyleEx_(wnd)
        , Wnd_GetDPI(wnd)));
    KK_VERIFY(::SetWindowPos(Hwnd(wnd)
        , nullptr
        , 0
        , 0
        , (r.right - r.left)
        , (r.bottom - r.top)
        , SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER));
}

bool Wnd_KeyState(OsWindow& /*wnd*/, Wnd_VirtualKey virtual_key)
{
    extern int Wnd_MapToVirtualKey(Wnd_VirtualKey vk_);
    const int win_vk = Wnd_MapToVirtualKey(virtual_key);
    if (win_vk == 0)
        return false;
    const SHORT state = ::GetAsyncKeyState(win_vk);
    return !!state;
}

static LRESULT Impl_WindowProc(OsWindow& wnd
    , Wnd_ProcState& state
    , Wnd_EventCallbacks& callback
    , UINT msg
    , WPARAM wparam
    , LPARAM lparam)
{
    switch (msg)
    {
    case WM_DPICHANGED:
    {
        const RECT* new_rect = reinterpret_cast<const RECT*>(lparam);
        // const WORD dpi_x = HIWORD(wparam);
        ::SetWindowPos(Hwnd(wnd), nullptr
            , new_rect->left, new_rect->top
            , new_rect->right - new_rect->left, new_rect->bottom - new_rect->top
            , SWP_NOZORDER | SWP_NOACTIVATE);
        callback.OnWndDPIChanged(wnd);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        const int cursor_x = GET_X_LPARAM(lparam);
        const int cursor_y = GET_Y_LPARAM(lparam);
        unsigned buttons = Wnd_MouseButtons::None;
        if (wparam & MK_LBUTTON)
            buttons |= Wnd_MouseButtons::Left;
        if (wparam & MK_RBUTTON)
            buttons |= Wnd_MouseButtons::Right;
        if (wparam & MK_MBUTTON)
            buttons |= Wnd_MouseButtons::Middle;
        callback.OnMouseMove(wnd, cursor_x, cursor_y, Wnd_MouseButtons{buttons});
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        const int cursor_x = GET_X_LPARAM(lparam);
        const int cursor_y = GET_Y_LPARAM(lparam);
        Wnd_MouseButtons buttons;
        Wnd_ButtonAction action = Wnd_ButtonAction::Press;
        switch (msg)
        {
        case WM_LBUTTONDOWN:
            buttons.mask = Wnd_MouseButtons::Left;
            action = Wnd_ButtonAction::Press;
            break;
        case WM_RBUTTONDOWN:
            buttons.mask = Wnd_MouseButtons::Right;
            action = Wnd_ButtonAction::Press;
            break;
        case WM_MBUTTONDOWN:
            buttons.mask = Wnd_MouseButtons::Middle;
            action = Wnd_ButtonAction::Press;
            break;
        case WM_LBUTTONUP:
            buttons.mask = Wnd_MouseButtons::Left;
            action = Wnd_ButtonAction::Release;
            break;
        case WM_RBUTTONUP:
            buttons.mask = Wnd_MouseButtons::Right;
            action = Wnd_ButtonAction::Release;
            break;
        case WM_MBUTTONUP:
            buttons.mask = Wnd_MouseButtons::Middle;
            action = Wnd_ButtonAction::Release;
            break;
        default:
            KK_VERIFY(false);
            break;
        }
        switch (action)
        {
        case Wnd_ButtonAction::Press:
            if (state.mouse_captured_count_++ == 0)
                (void)::SetCapture(Hwnd(wnd));
            break;
        case Wnd_ButtonAction::Release:
            if (--state.mouse_captured_count_ == 0)
                KK_VERIFY(::ReleaseCapture());
            break;
        case Wnd_ButtonAction::Repeat:
            break;
        }
        callback.OnMouseButton(wnd, buttons, action, Wnd_GetKeyModifiers(), cursor_x, cursor_y);
        return 0;
    }
    case WM_MOUSEWHEEL:
    {
        const short delta = short(HIWORD(wparam));
        callback.OnMouseVScroll(wnd, delta / float(WHEEL_DELTA));
        return 0;
    }
    case WM_MOUSEHWHEEL:
    {
        const short delta = short(HIWORD(wparam));
        callback.OnMouseHScroll(wnd, delta / float(WHEEL_DELTA));
        return 0;
    }
    case WM_UNICHAR:
    {
        if (wparam == UNICODE_NOCHAR)
            return TRUE;
        callback.OnInputCharacter(wnd, std::uint32_t(wparam), Wnd_GetKeyModifiers());
        return FALSE;
    }
    case WM_CHAR:
    case WM_SYSCHAR:
    {
        const std::uint32_t codepoint = Wnd_CodepointFrom_WM_CHAR(wnd, state, wparam);
        if (codepoint != std::uint32_t(-1))
        {
            // Mimic GLFW.
            if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
                return FALSE;
            callback.OnInputCharacter(wnd, codepoint, Wnd_GetKeyModifiers());
        }
        return FALSE;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        extern Wnd_VirtualKey Wnd_MapToVirtualKey(WPARAM wparam);
        Wnd_ButtonAction action = Wnd_ButtonAction::Press;
        if (msg == WM_KEYUP)
            action = Wnd_ButtonAction::Release;
        else if (msg == WM_KEYDOWN)
            action = Wnd_ButtonAction::Press;
        callback.OnKeyboard(wnd, Wnd_MapToVirtualKey(wparam), action, Wnd_GetKeyModifiers());
        return FALSE;
    }
    case WM_SETFOCUS:
    {
        callback.OnFocus(wnd, true);
        return 0;
    }
    case WM_KILLFOCUS:
    {
        callback.OnFocus(wnd, false);
        return 0;
    }
    case WM_WINDOWPOSCHANGED:
    {
        const WINDOWPOS* wnd_pos = reinterpret_cast<const WINDOWPOS*>(lparam);
        KK_VERIFY(wnd_pos);
        if (!(wnd_pos->flags & SWP_NOMOVE))
            callback.OnWndMove(wnd, wnd_pos->x, wnd_pos->y);
        if (!(wnd_pos->flags & SWP_NOSIZE))
        {
            // #TODO: Send client size instead of window size
            // to match what we do in Wnd_SetSize(). Unify all this.
            // callback.OnWndResize(wnd, wnd_pos->cx, wnd_pos->cy);
            const kk::Size client_size = Wnd_Size(wnd);
            callback.OnWndResize(wnd, client_size.width, client_size.height);
        }
        return 0;
    }
    }
    return ::DefWindowProc(Hwnd(wnd), msg, wparam, lparam);
}

void Wnd_HandleEventsTo(OsWindow& wnd, Wnd_EventCallbacks& handler)
{
    KK_VERIFY(!wnd.context.on_message);
    wnd.context.on_message = [&handler, state = Wnd_ProcState{}]
        (OsWindow& wnd, UINT msg, WPARAM wparam, LPARAM lparam) mutable
    {
        return Impl_WindowProc(wnd, state, handler, msg, wparam, lparam);
    };
}

// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
static const struct
{
    const int win_vk;
    const Wnd_VirtualKey vk;
} kVirtualKeys[] =
{
    {VK_LEFT, Wnd_VirtualKey::Left},
    {VK_RIGHT, Wnd_VirtualKey::Right},
    {VK_DOWN, Wnd_VirtualKey::Down},
    {VK_UP, Wnd_VirtualKey::Up},
    {VK_HOME, Wnd_VirtualKey::Home},
    {VK_END, Wnd_VirtualKey::End},
    {VK_PRIOR, Wnd_VirtualKey::PageUp},
    {VK_NEXT, Wnd_VirtualKey::PageDown},
    {VK_BACK, Wnd_VirtualKey::Backspace},
    {VK_DELETE, Wnd_VirtualKey::Delete},
    {VK_RETURN, Wnd_VirtualKey::Enter},
    {VK_MULTIPLY, Wnd_VirtualKey::Multiply},
    {VK_ADD, Wnd_VirtualKey::Add},
    {VK_SUBTRACT, Wnd_VirtualKey::Subtract},
    {VK_CONTROL, Wnd_VirtualKey::Ctrl},
    {VK_LBUTTON, Wnd_VirtualKey::MouseLeft},
    {0x58, Wnd_VirtualKey::X},
    {0x43, Wnd_VirtualKey::C},
    {0x56, Wnd_VirtualKey::V},
    {0x41, Wnd_VirtualKey::A},
    {0x4F, Wnd_VirtualKey::O},
    {0x5A, Wnd_VirtualKey::Z},
    {0x50, Wnd_VirtualKey::P},
    {0x49, Wnd_VirtualKey::I},
    {0x4E, Wnd_VirtualKey::N},
    {0x48, Wnd_VirtualKey::H},
    {0x44, Wnd_VirtualKey::D},
    {0x52, Wnd_VirtualKey::R},
    {0x4B, Wnd_VirtualKey::K},
};

Wnd_VirtualKey Wnd_MapToVirtualKey(WPARAM wparam)
{
    for (auto [win_vk, vk] : kVirtualKeys)
    {
        if (win_vk == int(wparam))
            return vk;
    }
    return Wnd_VirtualKey::Unmapped_;
}

int Wnd_MapToVirtualKey(Wnd_VirtualKey vk_)
{
    for (auto [win_vk, vk] : kVirtualKeys)
    {
        if (vk == vk_)
            return win_vk;
    }
    return 0;
}
#endif

#if (KK_WINDOW_GLFW())
static Wnd_Modifiers Wnd_GetKeyModifiers(GLFWwindow* window)
{
    Wnd_Modifiers m;

    if ((::glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        || (::glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS))
        m.mask |= Wnd_Modifiers::Shift;
    if ((::glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        || (::glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS))
        m.mask |= Wnd_Modifiers::Ctrl;
    if ((::glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS)
        || (::glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS))
        m.mask |= Wnd_Modifiers::Alt;
    if ((::glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS)
        || (::glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS))
        m.mask |= Wnd_Modifiers::Win;
    if (::glfwGetKey(window, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS)
        m.mask |= Wnd_Modifiers::CapsLock;
    if (::glfwGetKey(window, GLFW_KEY_NUM_LOCK) == GLFW_PRESS)
        m.mask |= Wnd_Modifiers::NumLock;

    return m;
}
static Wnd_Modifiers Wnd_GetKeyModifiers(int glfw_mods)
{
    Wnd_Modifiers m;

    if (glfw_mods & GLFW_MOD_SHIFT)
        m.mask |= Wnd_Modifiers::Shift;
    if (glfw_mods & GLFW_MOD_CONTROL)
        m.mask |= Wnd_Modifiers::Ctrl;
    if (glfw_mods & GLFW_MOD_ALT)
        m.mask |= Wnd_Modifiers::Alt;
    if (glfw_mods & GLFW_MOD_SUPER)
        m.mask |= Wnd_Modifiers::Win;
    if (glfw_mods & GLFW_MOD_CAPS_LOCK)
        m.mask |= Wnd_Modifiers::CapsLock;
    if (glfw_mods & GLFW_MOD_NUM_LOCK)
        m.mask |= Wnd_Modifiers::NumLock;
    return m;
}
static Wnd_MouseButtons Wnd_GetMouseButtons(GLFWwindow* window)
{
    Wnd_MouseButtons btns;
    if (::glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        btns.mask |= Wnd_MouseButtons::Left;
    if (::glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        btns.mask |= Wnd_MouseButtons::Right;
    if (::glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        btns.mask |= Wnd_MouseButtons::Middle;

    return btns;
}

GLFWwindow* Wnd_GLFW(OsWindow& wnd)
{
    GLFWwindow* glfw_wnd = wnd.context.glfw_wnd;
    KK_VERIFY(glfw_wnd);
    return glfw_wnd;
}

void Wnd_SetProcessDPIAware()
{
    // Handled by ::glfwInit().
}

unsigned Wnd_GetDPI(OsWindow& wnd)
{
#if (_MSC_VER)
    return unsigned(Wnd_GetDPIScale(wnd).x * USER_DEFAULT_SCREEN_DPI);
#else
    const unsigned kNonWindowsPlatformsDefaultDPI = 72; // 96
    return unsigned(Wnd_GetDPIScale(wnd).x * kNonWindowsPlatformsDefaultDPI);
#endif
}

kk::Vec2f Wnd_GetDPIScale(OsWindow& wnd)
{
#if (!KK_DISABLE_DPI())
    float x_dpi = 0.f;
    float y_dpi = 0.f;
    ::glfwGetWindowContentScale(Wnd_GLFW(wnd), &x_dpi, &y_dpi);
    return {x_dpi, y_dpi};
#else
    (void)wnd;
    return kk::Vec2f{1.f, 1.f};
#endif
}

void Wnd_AllEventsPoll()
{
    ::glfwPollEvents();
}

kk::Size Wnd_Size(OsWindow& wnd)
{
    kk::Size size{};
    ::glfwGetFramebufferSize(wnd.context.glfw_wnd, &size.width, &size.height);
    return size;
}

void Wnd_SetSize(OsWindow& wnd, int width, int height)
{
    ::glfwSetWindowSize(wnd.context.glfw_wnd, width, height);
}

void Wnd_Show(OsWindow& wnd)
{
    ::glfwShowWindow(wnd.context.glfw_wnd);
}

void Wnd_Hide(OsWindow& wnd)
{
    ::glfwHideWindow(wnd.context.glfw_wnd);
}

void Wnd_SetTitle(OsWindow& wnd, const char* title)
{
    ::glfwSetWindowTitle(wnd.context.glfw_wnd, title);
}

bool Wnd_IsAlive(OsWindow& wnd)
{
    return !::glfwWindowShouldClose(wnd.context.glfw_wnd);
}

void Wnd_SetClipboardTextUTF8(OsWindow& wnd, const std::string& text_utf8)
{
    ::glfwSetClipboardString(wnd.context.glfw_wnd, text_utf8.c_str());
}

std::string Wnd_GetClipboardTextUTF8(OsWindow& wnd)
{
    const char* text_utf8 = ::glfwGetClipboardString(wnd.context.glfw_wnd);
    if (text_utf8)
        return std::string(text_utf8);
    return std::string();
}

OsCursor* Wnd_LoadStandardCursor(Wnd_StandardCursor cursor_type)
{
    const int id = [&cursor_type]()
    {
        switch (cursor_type)
        {
        case Wnd_StandardCursor::Standard: return GLFW_ARROW_CURSOR;
        case Wnd_StandardCursor::Hand: return GLFW_HAND_CURSOR;
        case Wnd_StandardCursor::Size_DiagonalMain: return GLFW_CROSSHAIR_CURSOR;
        case Wnd_StandardCursor::Size_DiagonalSecondary: return GLFW_CROSSHAIR_CURSOR;
        case Wnd_StandardCursor::Size_Vertical: return GLFW_VRESIZE_CURSOR;
        case Wnd_StandardCursor::Size_Horizontal: return GLFW_HRESIZE_CURSOR;
        }
        return GLFW_ARROW_CURSOR;
    }();
    GLFWcursor* glfw_cursor = ::glfwCreateStandardCursor(id);
    return (OsCursor*)glfw_cursor;
}

void Wnd_SetCursor(OsWindow& wnd, OsCursor* cursor /*= nullptr*/)
{
    GLFWcursor* glfw_cursor = (GLFWcursor*)cursor;
    ::glfwSetCursor(wnd.context.glfw_wnd, glfw_cursor);
}

kk::Point Wnd_GetMousePosition(OsWindow& wnd)
{
    double xpos = 0;
    double ypos = 0;
    ::glfwGetCursorPos(wnd.context.glfw_wnd, &xpos, &ypos);
    kk::Point p{};
    p.x = int(std::round(xpos));
    p.y = int(std::round(ypos));
    return p;
}

kk::Point Wnd_GetPosition(OsWindow& wnd)
{
    kk::Point p{};
    ::glfwGetWindowPos(wnd.context.glfw_wnd, &p.x, &p.y);
    return p;
}

void Wnd_HandleEventsTo(OsWindow& wnd, Wnd_EventCallbacks& handler)
{
    KK_VERIFY(!wnd.context.glfw_wnd_callback);
    wnd.context.glfw_wnd_callback = &handler;

    (void)::glfwSetCursorPosCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, double xpos, double ypos)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        handler.OnMouseMove(wnd, int(xpos), int(ypos), Wnd_GetMouseButtons(window));
    });
    (void)::glfwSetScrollCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, double xoffset, double yoffset)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        if (xoffset != 0.0)
            handler.OnMouseHScroll(wnd, float(xoffset));
        if (yoffset != 0.0)
            handler.OnMouseVScroll(wnd, float(yoffset));
    });
    (void)::glfwSetMouseButtonCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, int button, int action, int mods)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        Wnd_MouseButtons btns{};
        Wnd_ButtonAction actns{};
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_RIGHT:   btns.mask |= Wnd_MouseButtons::Right; break;
        case GLFW_MOUSE_BUTTON_LEFT:    btns.mask |= Wnd_MouseButtons::Left; break;
        case GLFW_MOUSE_BUTTON_MIDDLE:  btns.mask |= Wnd_MouseButtons::Middle; break;
        default: return;
        }
        switch (action)
        {
        case GLFW_PRESS: actns = Wnd_ButtonAction::Press; break;
        case GLFW_RELEASE: actns = Wnd_ButtonAction::Release; break;
        default: KK_UNREACHABLE();
        }
        const kk::Point mouse_p = Wnd_GetMousePosition(wnd);
        handler.OnMouseButton(wnd, btns, actns, Wnd_GetKeyModifiers(mods), mouse_p.x, mouse_p.y);
    });
    (void)::glfwSetKeyCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, int key, int /*scancode*/, int action, int mods)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        Wnd_ButtonAction actns{};
        switch (action)
        {
        case GLFW_PRESS: actns = Wnd_ButtonAction::Press; break;
        case GLFW_RELEASE: actns = Wnd_ButtonAction::Release; break;
        case GLFW_REPEAT: actns = Wnd_ButtonAction::Repeat; break;
        default: KK_UNREACHABLE();
        }
        extern Wnd_VirtualKey Wnd_MapToVirtualKey(int glfw_key);
        handler.OnKeyboard(wnd, Wnd_MapToVirtualKey(key), actns, Wnd_GetKeyModifiers(mods));
    });
    (void)::glfwSetCharCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, unsigned int codepoint)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        handler.OnInputCharacter(wnd, codepoint, Wnd_GetKeyModifiers(window));
    });
    (void)::glfwSetWindowFocusCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, int focused)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        handler.OnFocus(wnd, (focused == GLFW_TRUE));
    });
    (void)::glfwSetWindowPosCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, int xpos, int ypos)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        handler.OnWndMove(wnd, xpos, ypos);
    });
    (void)::glfwSetWindowSizeCallback(Wnd_GLFW(wnd)
        , [](GLFWwindow* window, int width, int height)
    {
        OsWindow& wnd = *static_cast<OsWindow*>(glfwGetWindowUserPointer(window));
        Wnd_EventCallbacks& handler = *static_cast<Wnd_EventCallbacks*>(wnd.context.glfw_wnd_callback);
        handler.OnWndResize(wnd, width, height);
    });

    // #TODO:
    //virtual void OnWndDPIChanged(OsWindow & wnd);
}

void Wnd_SetPosition(OsWindow& wnd, int x, int y)
{
    ::glfwSetWindowPos(wnd.context.glfw_wnd, x, y);
}

bool Wnd_KeyState(OsWindow& wnd, Wnd_VirtualKey virtual_key)
{
    if (virtual_key == Wnd_VirtualKey::Left)
        return (::glfwGetMouseButton(wnd.context.glfw_wnd, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

    extern int Wnd_MapToVirtualKey(Wnd_VirtualKey vk_);
    return (::glfwGetKey(wnd.context.glfw_wnd, Wnd_MapToVirtualKey(virtual_key)) == GLFW_PRESS);
}

// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
static const struct
{
    const int glfw_vk;
    const Wnd_VirtualKey vk;
} kVirtualKeys[] =
{
    {GLFW_KEY_LEFT, Wnd_VirtualKey::Left},
    {GLFW_KEY_RIGHT, Wnd_VirtualKey::Right},
    {GLFW_KEY_DOWN, Wnd_VirtualKey::Down},
    {GLFW_KEY_UP, Wnd_VirtualKey::Up},
    {GLFW_KEY_HOME, Wnd_VirtualKey::Home},
    {GLFW_KEY_END, Wnd_VirtualKey::End},
    {GLFW_KEY_PAGE_UP, Wnd_VirtualKey::PageUp},
    {GLFW_KEY_PAGE_DOWN, Wnd_VirtualKey::PageDown},
    {GLFW_KEY_BACKSPACE, Wnd_VirtualKey::Backspace},
    {GLFW_KEY_DELETE, Wnd_VirtualKey::Delete},
    {GLFW_KEY_ENTER, Wnd_VirtualKey::Enter},
    {GLFW_KEY_KP_MULTIPLY, Wnd_VirtualKey::Multiply},
    {GLFW_KEY_KP_ADD, Wnd_VirtualKey::Add},
    {GLFW_KEY_KP_SUBTRACT, Wnd_VirtualKey::Subtract},
    {GLFW_KEY_LEFT_CONTROL, Wnd_VirtualKey::Ctrl},
    {GLFW_KEY_RIGHT_CONTROL, Wnd_VirtualKey::Ctrl},
    {-2, Wnd_VirtualKey::MouseLeft},
    {GLFW_KEY_X, Wnd_VirtualKey::X},
    {GLFW_KEY_C, Wnd_VirtualKey::C},
    {GLFW_KEY_V, Wnd_VirtualKey::V},
    {GLFW_KEY_A, Wnd_VirtualKey::A},
    {GLFW_KEY_O, Wnd_VirtualKey::O},
    {GLFW_KEY_Z, Wnd_VirtualKey::Z},
    {GLFW_KEY_P, Wnd_VirtualKey::P},
    {GLFW_KEY_I, Wnd_VirtualKey::I},
    {GLFW_KEY_N, Wnd_VirtualKey::N},
    {GLFW_KEY_H, Wnd_VirtualKey::H},
    {GLFW_KEY_D, Wnd_VirtualKey::D},
    {GLFW_KEY_R, Wnd_VirtualKey::R},
    {GLFW_KEY_K, Wnd_VirtualKey::K},
};

Wnd_VirtualKey Wnd_MapToVirtualKey(int glfw_key)
{
    for (auto [glfw_vk, vk] : kVirtualKeys)
    {
        if (glfw_vk == int(glfw_key))
            return vk;
    }
    return Wnd_VirtualKey::Unmapped_;
}

int Wnd_MapToVirtualKey(Wnd_VirtualKey vk_)
{
    for (auto [glfw_vk, vk] : kVirtualKeys)
    {
        if (vk == vk_)
            return glfw_vk;
    }
    return 0;
}
#endif
