#if defined(_MSC_VER)
#  include <Windows.h>
#endif

#include "os_render_backend.hh"
#include "os_window.hh"
#include "os_window_UTILS.hh"

#if (KK_WINDOW_GLFW())
#  include <GLFW/glfw3.h>
#endif

#include <cmath>
#include <cstdio>

#if (!KK_WINDOW_OPENGL())
#  error OpenGL Render requires Window with OpenGL context (KK_WINDOW_OPENGL() == 1).
#endif

static float N_(std::uint8_t c)
{
    return (c / 255.f);
}

static const char* GL_DebugSource_ToStr(GLenum source)
{
    switch (source)
    {
    case GL_DEBUG_SOURCE_API:             return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY:     return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION:     return "APPLICATION";
    case GL_DEBUG_SOURCE_OTHER:           return "OTHER";
    }
    return "";
}

static const char* GL_DebugType_ToStr(GLenum type)
{
    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:               return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY:         return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE:         return "PERFORMANCE";
    case GL_DEBUG_TYPE_MARKER:              return "MARKER";
    case GL_DEBUG_TYPE_PUSH_GROUP:          return "PUSH_GROUP";
    case GL_DEBUG_TYPE_POP_GROUP:           return "POP_GROUP";
    case GL_DEBUG_TYPE_OTHER:               return "OTHER";
    }
    return "";
}

static const char* GL_DebugSeverity_ToStr(GLenum severity)
{
    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:         return "HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM:       return "MEDIUM";
    case GL_DEBUG_SEVERITY_LOW:          return "LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
    }
    return "";
}

static const unsigned kIgnoreLogIdList[] =
{
    131185, // Buffer detailed info: Buffer object 1 (bound to GL_ARRAY_BUFFER_ARB, usage hint is GL_STATIC_DRAW) will use VIDEO memory as the source for buffer object operations.
    131218, // Program/shader state performance warning: Vertex shader in program 3 is being recompiled based on GL state.
#if (0)
    131169,
    131204,
#endif
};

static void GL_DebugOutput(GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei /*length*/,
    const char* message,
    const void* /*userParam*/)
{
    for (unsigned to_ignore : kIgnoreLogIdList)
    {
        if (to_ignore == id)
            return;
    }

    fprintf(stderr, "[%u][%s][%s][%s] %s.\n"
        , id
        , GL_DebugSource_ToStr(source)
        , GL_DebugType_ToStr(type)
        , GL_DebugSeverity_ToStr(severity)
        , message);
    fflush(stderr);
}

static void GL_SetContext(OsWindow& window)
{
#if (KK_WINDOW_WIN32())
    KK_VERIFY(::wglMakeCurrent((HDC)window.context.hdc, (HGLRC)window.context.hglrc));
#elif (KK_WINDOW_GLFW())
    ::glfwMakeContextCurrent(window.context.glfw_wnd);
#endif
}

OsRender_State* OsRender_Create()
{
    static int no_state;
    return &no_state;
}

void OsRender_WindowCreate(OsRender_State*, OsWindow& window)
{
    GL_SetContext(window);
    int flags = 0;
    ::glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        ::glEnable(GL_DEBUG_OUTPUT);
        ::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        ::glDebugMessageCallback(GL_DebugOutput, nullptr);
        ::glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }

    ::glEnable(GL_BLEND);
    ::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ::glEnable(GL_MULTISAMPLE);
}

void OsRender_WindowDestroy(OsRender_State*, OsWindow&)
{
}

void OsRender_Build(OsRender_State*, OsWindow& window, kr::KidsRender& render)
{
    GL_SetContext(window);
    kr::RenderData render_data{};
    kr::KidsRender::Build(render_data, render);
}

void OsRender_NewFrame(OsRender_State*
    , OsWindow& window
    , const kk::Color& clear_color
    , OsRender_NewFrameCallback render_frame_callback)
{
    GL_SetContext(window);
    const kk::Size wnd_size = Wnd_Size(window);

    ::glViewport(0, 0, wnd_size.width, wnd_size.height);
    ::glClearColor(N_(clear_color.r), N_(clear_color.g), N_(clear_color.b), N_(clear_color.a));
    ::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    kr::FrameInfo frame_info{};
    frame_info.screen_size = wnd_size;
    render_frame_callback(window, frame_info);
}

void OsRender_Present(OsRender_State*, OsWindow& window)
{
    GL_SetContext(window);

#if (KK_WINDOW_WIN32())
    ::SwapBuffers((HDC)window.context.hdc);
#elif (KK_WINDOW_GLFW())
    ::glfwSwapBuffers(window.context.glfw_wnd);
#endif
}
void OsRender_Finish(OsRender_State*)
{
}
void OsRender_Destroy(OsRender_State*)
{
}
