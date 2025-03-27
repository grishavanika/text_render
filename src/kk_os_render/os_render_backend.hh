#pragma once
#include "KR_kids_render.hh"
#include <functional>

#if (KK_RENDER_OPENGL())
#  define KK_RENDER_NAME() "OpenGL"
#elif (KK_RENDER_VULKAN())
#  define KK_RENDER_NAME() "Vulkan"
#else
#  error Unknown render backend.
#endif

class OsWindow;

using OsRender_State = void;
using OsRender_NewFrameCallback = std::function<void (OsWindow& window, kr::FrameInfo& frame_info)>;

OsRender_State* OsRender_Create();
void OsRender_WindowCreate(OsRender_State* void_state, OsWindow& window);
void OsRender_Build(OsRender_State* void_state, OsWindow& window, kr::KidsRender& render);
void OsRender_NewFrame(OsRender_State* void_state
    , OsWindow& window
    , const kk::Color& clear_color
    , OsRender_NewFrameCallback render_frame_callback);
void OsRender_Present(OsRender_State* void_state, OsWindow& window);
void OsRender_Finish(OsRender_State* void_state);
void OsRender_WindowDestroy(OsRender_State* void_state, OsWindow& window);
void OsRender_Destroy(OsRender_State* void_state);

struct OsRender
{
    OsRender_State* state = nullptr;

    OsRender()
        : state{OsRender_Create()}
    {
    }

    ~OsRender()
    {
        if (state)
            OsRender_Destroy(state);
    }

    OsRender(const OsRender&) noexcept = delete;
    OsRender& operator=(const OsRender&) noexcept = delete;
    OsRender& operator=(OsRender&&) noexcept = delete;
    OsRender(OsRender&& rhs) noexcept
        : state(rhs.state)
    {
        rhs.state = nullptr;
    }
};
