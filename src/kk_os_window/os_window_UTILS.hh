#pragma once
#include "KS_basic_math.hh"
#include <string>

#define KK_DISABLE_DPI() 0

class OsWindow;
struct Wnd_EventCallbacks;

struct Wnd_Modifiers;
struct Wnd_MouseButtons;
struct OsCursor;

enum class Wnd_VirtualKey : unsigned;
enum class Wnd_StandardCursor : unsigned;
enum class Wnd_ButtonAction : unsigned;

void Wnd_AllEventsPoll();
void Wnd_HandleEventsTo(OsWindow& wnd, Wnd_EventCallbacks& handler);
kk::Size Wnd_Size(OsWindow& wnd);
void Wnd_SetSize(OsWindow& wnd, int width, int height);
std::string Wnd_OpenFileDialog_Blocking(OsWindow& wnd);
void Wnd_Show(OsWindow& wnd);
void Wnd_Hide(OsWindow& wnd);
void Wnd_SetTitle(OsWindow& wnd, const char* title);
bool Wnd_IsAlive(OsWindow& wnd);
void Wnd_SetClipboardTextUTF8(OsWindow& wnd, const std::string& text_utf8);
std::string Wnd_GetClipboardTextUTF8(OsWindow& wnd);
OsCursor* Wnd_LoadStandardCursor(Wnd_StandardCursor cursor_type);
void Wnd_SetCursor(OsWindow& wnd, OsCursor* cursor = nullptr);
kk::Point Wnd_GetMousePosition(OsWindow& wnd);
kk::Point Wnd_GetPosition(OsWindow& wnd);
void Wnd_SetPosition(OsWindow& wnd, int x, int y);
bool Wnd_KeyState(OsWindow& wnd, Wnd_VirtualKey virtual_key);
void Wnd_SetProcessDPIAware();
unsigned Wnd_GetDPI(OsWindow& wnd);
kk::Vec2f Wnd_GetDPIScale(OsWindow& wnd);

struct Wnd_EventCallbacks
{
    virtual void OnMouseMove(OsWindow& wnd, int cursor_x, int cursor_y, Wnd_MouseButtons buttons);
    virtual void OnMouseVScroll(OsWindow& wnd, float delta);
    virtual void OnMouseHScroll(OsWindow& wnd, float delta);
    virtual void OnMouseButton(OsWindow& wnd, Wnd_MouseButtons buttons, Wnd_ButtonAction action, Wnd_Modifiers modifiers, int mouse_x, int mouse_y);
    virtual void OnKeyboard(OsWindow& wnd, Wnd_VirtualKey virtual_key, Wnd_ButtonAction action, Wnd_Modifiers modifiers);
    virtual void OnInputCharacter(OsWindow& wnd, std::uint32_t codepoint, Wnd_Modifiers modifiers);
    virtual void OnFocus(OsWindow& wnd, bool has_focus);
    virtual void OnWndMove(OsWindow& wnd, int x, int y);
    virtual void OnWndResize(OsWindow& wnd, int width, int height);
    virtual void OnWndDPIChanged(OsWindow& wnd);

protected:
    ~Wnd_EventCallbacks() = default;
};

struct Wnd_Modifiers
{
    enum Mask : unsigned
    {
        None       = 0,
        Shift      = 1 << 1,
        Ctrl       = 1 << 2,
        Alt        = 1 << 3,
        Win        = 1 << 4,
        CapsLock   = 1 << 5,
        NumLock    = 1 << 6,
    };
    unsigned mask = Mask::None;

    bool check(Mask m) const { return ((mask & m) == m); }
};

struct Wnd_MouseButtons
{
    enum Mask : unsigned
    {
        None   = 0,
        Left   = 1 << 1,
        Right  = 1 << 2,
        Middle = 1 << 3,
    };
    unsigned mask = Mask::None;
    
    bool check(Mask m) const { return ((mask & m) == m); }
};

enum class Wnd_ButtonAction : unsigned
{
    Press,
    Release,
    Repeat,
};

enum class Wnd_StandardCursor : unsigned
{
    Standard,
    Hand,
    Size_DiagonalMain,
    Size_DiagonalSecondary,
    Size_Vertical,
    Size_Horizontal,
};

enum class Wnd_VirtualKey : unsigned
{
    Unmapped_ = 0,
    Left,
    Right,
    Down,
    Up,
    Home,
    End,
    PageUp,
    PageDown,
    Backspace,
    Delete,
    Enter,
    Multiply,
    Subtract,
    Add,
    Ctrl,
    MouseLeft,
    X,
    C,
    V,
    A,
    O,
    Z,
    P,
    I,
    N,
    H,
    D,
    R,
    K,
};
