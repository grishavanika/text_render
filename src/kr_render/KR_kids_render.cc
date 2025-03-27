#include "KR_kids_render.hh"
#include "KR_kids_font.hh"

#include <utility>
#include <cstring>
#include <cstddef>
#include <cmath>

namespace kr
{

static float Normal_U8(std::uint8_t c)
{
    return (c / 255.f);
}

static Vertex Vertex_Make(float x, float y, const kk::Color& color)
{
    Vertex v{};
    v.uv_ = kk::Vec2f{};
    v.p_.x = float(x);
    v.p_.y = float(y);
    v.c_.r = Normal_U8(color.r);
    v.c_.g = Normal_U8(color.g);
    v.c_.b = Normal_U8(color.b);
    v.c_.a = Normal_U8(color.a);
    return v;
}

static kk::Point2f ImBezierCubicCalc(const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Point2f& p3
    , const kk::Point2f& p4
    , float t)
{
    const float u = 1.0f - t;
    const float w1 = u * u * u;
    const float w2 = 3 * u * u * t;
    const float w3 = 3 * u * t * t;
    const float w4 = t * t * t;
    return kk::Vec2f
    {
        .x = w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x,
        .y = w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y
    };
}

static kk::Point2f ImBezierQuadraticCalc(const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Point2f& p3
    , float t)
{
    const float u = 1.f - t;
    const float w1 = u * u;
    const float w2 = 2 * u * t;
    const float w3 = t * t;
    return kk::Vec2f
    {
        .x = w1 * p1.x + w2 * p2.x + w3 * p3.x,
        .y = w1 * p1.y + w2 * p2.y + w3 * p3.y
    };
}

static ImageRef Texture_White_1x1(KidsRender& render)
{
    unsigned data = 0xffffffff;
    return ImageRef::FromMemory(render, ImageRef::Format::RGBA, 1, 1, &data);
}

static bool DrawCmd_CanMerge(const DrawCmd& prev_cmd
    , const ImageRef& texture
    , const ClipRect& clip_rect
    , const kk::Vec2f& scale)
{
    return (prev_cmd.texture_ == texture)
        && (prev_cmd.clip_rect_ == clip_rect)
        && (prev_cmd.scale_ == scale);
}

static float Scale(float v, float scale)
{
    return (v * scale);
}

static kk::Rect2f Scale(const kk::Rect2f& r, const kk::Vec2f& scale)
{
    return
    {
        .x = Scale(r.x, scale.x),
        .y = Scale(r.y, scale.y),
        .width = Scale(r.width, scale.x),
        .height = Scale(r.height, scale.y),
    };
}

static kk::Rect ClipRect_RoundPx(const kk::Rect2f& r)
{
    kk::Rect r_int;
    r_int.x = int(std::floor(r.x));
    r_int.y = int(std::floor(r.y));
    r_int.width = int(std::ceil(r.width));
    r_int.height = int(std::ceil(r.height));
    return r_int;
}

static kk::Rect ClipRect_Transform(const ClipRect& clip_rect, const kk::Vec2f& scale, const kk::Size& screen_size)
{
    if (clip_rect == ClipRect{})
    {
        kk::Rect no_clip;
        no_clip.width = screen_size.width;
        no_clip.height = screen_size.height;
        return no_clip;
    }
    return ClipRect_RoundPx(Scale(clip_rect, scale));
}

/*static*/ void KidsRender::AddVertices(CmdList& cmd_list
    , const ImageRef& texture
    , const std::span<const Vertex>& new_vertices
    , const std::span<const Index>& new_indices
    , const ClipRect& clip_rect
    , const kk::Vec2f& scale
    )
{
    std::vector<DrawCmd>& draw_list = cmd_list.draw_list_;
    std::vector<Vertex>& vertex_list = cmd_list.vertex_list_;
    std::vector<Index>& index_list = cmd_list.index_list_;

    const unsigned vertex_base = unsigned(vertex_list.size());
    const unsigned index_base = unsigned(index_list.size());

    DrawCmd* cmd = nullptr;
    if ((draw_list.size() > 0)
        && DrawCmd_CanMerge(draw_list.back(), texture, clip_rect, scale))
        cmd = &draw_list.back();
    if (!cmd)
    {
        draw_list.push_back({});
        cmd = &draw_list.back();
        cmd->index_offset_ = index_base;
        cmd->vertex_offset_ = vertex_base;
        cmd->texture_ = texture;
        cmd->clip_rect_ = clip_rect;
        cmd->scale_ = scale;
    }
    cmd->index_count_ += unsigned(new_indices.size());
    cmd->vertex_count_ += unsigned(new_vertices.size());

    vertex_list.insert(vertex_list.end()
        , new_vertices.begin(), new_vertices.end());
    index_list.resize(index_list.size() + new_indices.size());
    for (std::size_t i = 0; i < new_indices.size(); ++i)
        index_list[index_base + i] = Index(new_indices[i] + vertex_base);
}

// Lowest-level API: push whatever makes sense.
void KidsRender::merge_cmd_lists(const CmdList& new_cmd_list
    , const kk::Point2f* translate_by // = nullptr
    , const ClipRect* override_clip_rect // = nullptr
    , CmdList* append_to_cmd_list // = nullptr
    )
{
    // #TODO: merge with AddVertices().
    CmdList& cmd_list = append_to_cmd_list
        ? *append_to_cmd_list
        : cmd_list_;

    std::vector<DrawCmd>& draw_list = cmd_list.draw_list_;
    std::vector<Vertex>& vertex_list = cmd_list.vertex_list_;
    std::vector<Index>& index_list = cmd_list.index_list_;

    const unsigned vertex_base = unsigned(vertex_list.size());
    const unsigned index_base = unsigned(index_list.size());

    const std::vector<Vertex>& new_vertex_list = new_cmd_list.vertex_list_;
    const std::vector<Index>& new_index_list = new_cmd_list.index_list_;

    if (!translate_by)
        vertex_list.insert(vertex_list.end(), new_vertex_list.begin(), new_vertex_list.end());
    else
    {
        const std::size_t count = vertex_list.size();
        vertex_list.resize(count + new_vertex_list.size());
        for (std::size_t i = 0, new_count = new_vertex_list.size(); i < new_count; ++i)
        {
            Vertex& v = vertex_list[i + count];
            v = new_vertex_list[i];
            v.p_.x += translate_by->x;
            v.p_.y += translate_by->y;
        }
    }
    index_list.resize(index_list.size() + new_index_list.size());
    for (std::size_t i = 0; i < new_index_list.size(); ++i)
        index_list[index_base + i] = Index(new_index_list[i] + vertex_base);

    for (const DrawCmd& new_cmd : new_cmd_list.draw_list_)
    {
        const ClipRect clip_rect = override_clip_rect
            ? *override_clip_rect
            : new_cmd.clip_rect_;
        DrawCmd* cmd = nullptr;
        if ((draw_list.size() > 0)
            && DrawCmd_CanMerge(draw_list.back(), new_cmd.texture_, clip_rect, new_cmd.scale_))
            cmd = &draw_list.back();
        if (!cmd)
        {
            draw_list.push_back({});
            cmd = &draw_list.back();
            cmd->index_offset_ = index_base + new_cmd.index_offset_;
            cmd->vertex_offset_ = vertex_base + new_cmd.vertex_offset_;
            cmd->texture_ = new_cmd.texture_;
            cmd->clip_rect_ = clip_rect;
            cmd->scale_ = new_cmd.scale_;
        }
        cmd->index_count_ += unsigned(new_cmd.index_count_);
        cmd->vertex_count_ += unsigned(new_cmd.vertex_count_);
    }
}

void KidsRender::line(const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Color& color    // = Color_White()
    , float width               // = 1.f
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    KK_ASSERT(width > 0.f);

    auto to_vertex = [&color](const kk::Point2f& p) -> Vertex
    {
        return Vertex_Make(p.x, p.y, color);
    };

    auto to_edge = [&](const kk::Point2f& p) -> Vertex
    {
        bool antialiased = false;
        return antialiased
            ? Vertex_Make(p.x, p.y, {0xff, 0xff, 0xff, 0x00})
            : Vertex_Make(p.x, p.y, color);
    };

    const float width_x = (width /*/ scale.x*/) / 2.f;
    const float width_y = (width /*/ scale.y*/) / 2.f;

    const kk::Vec2f d = (p2 - p1);
    const kk::Vec2f n0{+d.y, -d.x};
    kk::Vec2f n = Vec2f_Normalize(n0);
    n.x *= width_x;
    n.y *= width_y;

    const kk::Point2f p11  = (p1 + n);
    const kk::Point2f p21  = (p2 + n);
    const kk::Point2f p11n = (p1 - n);
    const kk::Point2f p21n = (p2 - n);

    const Vertex vertices[] =
    {
        to_vertex(p1),
        to_vertex(p2),
        to_edge(p11),
        to_edge(p21),
        to_edge(p11n),
        to_edge(p21n),
    };
    const Index indices[] =
    {
        0, 2, 1,
        2, 3, 1,
        0, 4, 1,
        4, 5, 1,
    };
    AddVertices(cmd_list ? *cmd_list : cmd_list_
        , white_1x1_
        , vertices
        , indices
        , clip_rect
        , scale);
}

void KidsRender::triangle(const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Point2f& p3
    , const kk::Color& color    // = Color_White()
    , float width               // = 1.f
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    line(p1, p2, color, width, scale, clip_rect, cmd_list);
    line(p2, p3, color, width, scale, clip_rect, cmd_list);
    line(p3, p1, color, width, scale, clip_rect, cmd_list);
}

void KidsRender::triangle_fill(const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Point2f& p3
    , const kk::Color& color    // = Color_White()
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    auto to_ = [&color](float x, float y) { return Vertex_Make(x, y, color); };

    const Vertex vertices[] =
    {
        to_(p1.x, p1.y),
        to_(p2.x, p2.y),
        to_(p3.x, p3.y),
    };
    const Index indices[] =
    {
        0,
        1,
        2,
    };
    AddVertices(cmd_list ? *cmd_list : cmd_list_
        , white_1x1_
        , vertices
        , indices
        , clip_rect
        , scale);
}

void KidsRender::rect(const kk::Point2f& p_min_
    , const kk::Point2f& p_max_
    , const kk::Color& color    // = Color_White()
    , float width               // = 1.f
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    // So the line is not clipped, see how line() is rendered.
    const kk::Vec2f delta = kk::Vec2f{0.5f, 0.5f} * width;
    const kk::Point2f p_min = (p_min_ + delta);
    const kk::Point2f p_max = (p_max_ - delta);

    line(p_min, {p_max.x, p_min.y}, color, width, scale, clip_rect, cmd_list);
    line({p_max.x, p_min.y}, p_max, color, width, scale, clip_rect, cmd_list);
    line(p_max, {p_min.x, p_max.y}, color, width, scale, clip_rect, cmd_list);
    line({p_min.x, p_max.y}, p_min, color, width, scale, clip_rect, cmd_list);
}

void KidsRender::rect_fill(const kk::Point2f& p_min
    , const kk::Point2f& p_max
    , const kk::Color& color    // = Color_White()
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    auto to_ = [&color](float x, float y) { return Vertex_Make(x, y, color); };

    const float x1 = p_min.x;
    const float x2 = p_max.x;
    const float y1 = p_min.y;
    const float y2 = p_max.y;

    const Vertex vertices[] =
    {
        to_(x1, y1),
        to_(x1, y2),
        to_(x2, y2),
        to_(x2, y1),
    };
    const Index indices[] =
    {
        0,
        1,
        2,
        2,
        3,
        0,
    };
    AddVertices(cmd_list ? *cmd_list : cmd_list_
        , white_1x1_
        , vertices
        , indices
        , clip_rect
        , scale);
}

void KidsRender::circle_impl(const kk::Point2f& p_center
    , float radius
    , bool do_fill
    , const kk::Color& color
    , int segments_count
    , const ClipRect& clip_rect
    , CmdList* cmd_list
    , const kk::Vec2f& scale
    , float width
    )
{
    if (segments_count <= 0)
        segments_count = 314; // Auto-detect.
    KK_VERIFY(radius >= 0);

    auto segment = [&](int n) -> kk::Point2f
    {
        const double theta = ((2.0 * 3.1415926 * n) / segments_count);
        const double x = ((radius * std::cos(theta)));
        const double y = (radius * std::sin(theta));

        const float x_px = float(std::round(x) + p_center.x);
        const float y_px = float(std::round(y) + p_center.y);
        return {x_px, y_px};
    };

    const kk::Point2f first = segment(0);
    kk::Point2f prev = first;

    auto render_ = [&](kk::Point2f new_)
    {
        if (do_fill)
        {
            triangle_fill(p_center
#if (KK_RENDER_VULKAN()) // Please, fix me.
                , new_, prev
#else
                , prev, new_
#endif
                , color
                , scale
                , clip_rect
                , cmd_list);
        }
        else
            line(prev, new_, color, width, scale, clip_rect, cmd_list);
        prev = new_;
    };

    for (int ii = 1; ii < segments_count; ++ii)
        render_(segment(ii));

    render_(first);
}

void KidsRender::circle(const kk::Point2f& p_center
    , float radius
    , const kk::Color& color    // = Color_White()
    , float width               // = 1.f
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , int segments_count        // = -1
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    const bool do_fill = false;
    return circle_impl(p_center
        , radius
        , do_fill
        , color
        , segments_count
        , clip_rect
        , cmd_list
        , scale
        , width);
}

void KidsRender::circle_fill(const kk::Point2f& p_center
    , float radius
    , const kk::Color& color    // = Color_White()
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , int segments_count        // = -1
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    const bool do_fill = true;
    return circle_impl(p_center
        , radius
        , do_fill
        , color
        , segments_count
        , clip_rect
        , cmd_list
        , scale
        , 1.f);
}

void KidsRender::image(const ImageRef& image
    , const kk::Point2f& p_min  // = kk::Point2f{0, 0}
    , const kk::Point2f& p_max  // = kk::Point2f_Invalid() // Use image's size.
    , const kk::Vec2f& uv_min   // = kk::Vec2f{0.f, 0.f} // Use full image to sample.
    , const kk::Vec2f& uv_max   // = kk::Vec2f{1.f, 1.f}
    , const kk::Color& color_   // = Color_White()
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    KK_VERIFY(image.is_valid());

    const kk::Vec2f a = p_min;
    kk::Vec2f c = p_max;
    if (p_max == kk::Point2f_Invalid())
    {
        c.x = a.x + float(image.width()); // #TODO: unscale?
        c.y = a.y + float(image.height());
    };

    const kk::Vec2f uv_a = uv_min;
    const kk::Vec2f uv_c = uv_max;
    const kk::Vec2f b{c.x, a.y};
    const kk::Vec2f d{a.x, c.y};
    const kk::Vec2f uv_b{uv_c.x, uv_a.y};
    const kk::Vec2f uv_d{uv_a.x, uv_c.y};

    kk::Vec4f color{};
    color.r = Normal_U8(color_.r);
    color.g = Normal_U8(color_.g);
    color.b = Normal_U8(color_.b);
    color.a = Normal_U8(color_.a);

    auto to_ = [&color](kk::Vec2f p, kk::Vec2f uv) { return Vertex{p, uv, color}; };
    const Vertex vertices[] =
    {
        to_(c, uv_c),
        to_(b, uv_b),
        to_(a, uv_a),
        to_(d, uv_d),
    };
    const Index indices[6] =
    {
        0,
        1,
        2,
        2,
        3,
        0,
    };
    AddVertices(cmd_list ? *cmd_list : cmd_list_
        , image
        , vertices
        , indices
        , clip_rect
        , scale);
}

void KidsRender::bezier_cubic(
      const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Point2f& p3
    , const kk::Point2f& p4
    , const kk::Color& color     // = Color_White()
    , float width                // = 1.f
    , int segments_count         // = -1
    , const kk::Vec2f& scale     // = kk::Vec2f{1.f, 1.f}
    , const ClipRect& clip_rect  // = {}
    , CmdList* cmd_list          // = nullptr
    )
{
    if (segments_count <= 0)
        segments_count = 128; // Auto-detect.

    const float t_step = (1.f / segments_count);
    auto next_ = [&](int n) -> kk::Point2f
    {
        return ImBezierCubicCalc(p1, p2, p3, p4, t_step * n);
    };

    const kk::Point2f first = next_(0);
    kk::Point2f prev = first;

    auto render_ = [&](kk::Point2f new_)
    {
        line(prev, new_, color, width, scale, clip_rect, cmd_list);
        prev = new_;
    };

    for (int i_step = 1; i_step <= segments_count; ++i_step)
        render_(next_(i_step));
}

void KidsRender::bezier_quadratic(
    const kk::Point2f& p1
    , const kk::Point2f& p2
    , const kk::Point2f& p3
    , const kk::Color& color    // = Color_White()
    , float width               // = 1.f
    , const kk::Vec2f& scale    // = kk::Vec2f{1.f, 1.f}
    , int segments_count        // = -1
    , const ClipRect& clip_rect // = {}
    , CmdList* cmd_list         // = nullptr
    )
{
    if (segments_count <= 0)
        segments_count = 128; // Auto-detect.

    const float t_step = (1.f / segments_count);
    auto next_ = [&](int n) -> kk::Point2f
    {
        return ImBezierQuadraticCalc(p1, p2, p3, t_step * n);
    };

    const kk::Point2f first = next_(0);
    kk::Point2f prev = first;

    auto render_ = [&](kk::Point2f new_)
    {
        line(prev, new_, color, width, scale, clip_rect, cmd_list);
        prev = new_;
    };

    for (int i_step = 1; i_step <= segments_count; ++i_step)
        render_(next_(i_step));
}

void KidsRender::clear()
{
    cmd_list_ = {};
}

#if (KK_RENDER_OPENGL())
static const char kShader_Vertex[] =
R"(
#version 430 core

layout (location = 0) in vec2 IN_Position;
layout (location = 1) in vec2 IN_UV;
layout (location = 2) in vec4 IN_Color;

uniform float ScreenWidth;
uniform float ScreenHeight;
uniform float ScaleX;
uniform float ScaleY;

out vec2 Frag_UV;
out vec4 Frag_Color;

mat3x3 ortho2d(float screen_width, float screen_height)
{
    float left = 0;
    float right = screen_width;
    float top = 0;
    float bottom = screen_height;

    mat3x3 m = mat3x3(1);
    m[0][0] = 2 / (right - left); // scale
    m[1][1] = 2 / (top - bottom);
    m[2][0] = -(right + left) / (right - left); // move/center
    m[2][1] = -(top + bottom) / (top - bottom);
    return m;
}

mat3x3 scale2d(float scale_x, float scale_y)
{
    mat3x3 m = mat3x3(1);
    m[0][0] = scale_x;
    m[1][1] = scale_y;
    return m;
}

void main()
{
    mat3x3 ortho = ortho2d(ScreenWidth, ScreenHeight);
    mat3x3 scale = scale2d(ScaleX, ScaleY);
    vec3 p = ortho * scale * vec3(IN_Position, 1.0f);
    gl_Position = vec4(p, 1.0f);

    Frag_Color = IN_Color;
    Frag_UV = IN_UV;
}
)";

static const char kShader_Fragment[] =
R"(
#version 430 core

uniform sampler2D Texture;

in vec2 Frag_UV;
in vec4 Frag_Color;

out vec4 Out_Color;

void main()
{
    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);
}
)";

static unsigned Shaders_Link(const char* vertex_src, const char* fragment_src)
{
    unsigned int vertex_shader = ::glCreateShader(GL_VERTEX_SHADER);
    ::glShaderSource(vertex_shader, 1, &vertex_src, nullptr);
    ::glCompileShader(vertex_shader);
    {
        int success = -1;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        KK_VERIFY(success != 0);
    }
    unsigned int fragment_shader = ::glCreateShader(GL_FRAGMENT_SHADER);
    ::glShaderSource(fragment_shader, 1, &fragment_src, nullptr);
    ::glCompileShader(fragment_shader);
    {
        int success = -1;
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        KK_VERIFY(success != 0);
    }

    unsigned shader_program = ::glCreateProgram();
    ::glAttachShader(shader_program, vertex_shader);
    ::glAttachShader(shader_program, fragment_shader);
    ::glLinkProgram(shader_program);
    {
        int success = -1;
        glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
        KK_VERIFY(success != 0);
    }
    ::glDeleteShader(vertex_shader);
    ::glDeleteShader(fragment_shader);
    return shader_program;
}

static void Shaders_Free(unsigned shader_program)
{
    ::glDeleteProgram(shader_program);
}

KidsRender::KidsRender() = default;

KidsRender::~KidsRender() noexcept
{
    Shaders_Free(shader_program_);
}

/*static*/ void KidsRender::Build(const RenderData& render_data, KidsRender& render)
{
    render.render_data_ = render_data;

    render.shader_program_ = Shaders_Link(kShader_Vertex, kShader_Fragment);

    render.screen_width_ptr_ = ::glGetUniformLocation(render.shader_program_, "ScreenWidth");
    render.screen_height_ptr_ = ::glGetUniformLocation(render.shader_program_, "ScreenHeight");
    render.scale_x_ptr_ = ::glGetUniformLocation(render.shader_program_, "ScaleX");
    render.scale_y_ptr_ = ::glGetUniformLocation(render.shader_program_, "ScaleY");
    render.texture_ptr_ = ::glGetUniformLocation(render.shader_program_, "Texture");
    KK_VERIFY(render.screen_width_ptr_ >= 0);
    KK_VERIFY(render.screen_height_ptr_ >= 0);
    KK_VERIFY(render.scale_x_ptr_ >= 0);
    KK_VERIFY(render.scale_y_ptr_ >= 0);
    KK_VERIFY(render.texture_ptr_ >= 0);

    render.white_1x1_ = Texture_White_1x1(render);
}

void KidsRender::draw(const FrameInfo& frame_info)
{
    if (cmd_list_.draw_list_.empty())
        return;

    auto upload_vertices = [this](unsigned int* VBO, unsigned int* VAO, unsigned int* EBO)
    {
        KK_VERIFY(cmd_list_.vertex_list_.size() > 0);
        ::glGenVertexArrays(1, VAO);
        ::glGenBuffers(1, VBO);
        ::glGenBuffers(1, EBO);
        ::glBindVertexArray(*VAO);
        ::glBindBuffer(GL_ARRAY_BUFFER, *VBO);
        ::glBufferData(GL_ARRAY_BUFFER
            , cmd_list_.vertex_list_.size() * sizeof(Vertex)
            , &cmd_list_.vertex_list_[0]
            , GL_STATIC_DRAW);
        ::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *EBO);
        ::glBufferData(GL_ELEMENT_ARRAY_BUFFER
            , cmd_list_.index_list_.size() * sizeof(Index)
            , &cmd_list_.index_list_[0]
            , GL_STATIC_DRAW);

        ::glVertexAttribPointer(0
            , sizeof(Vertex::p_) / sizeof(float)
            , GL_FLOAT
            , GL_FALSE
            , sizeof(Vertex)
            , (void*)offsetof(Vertex, p_));
        ::glEnableVertexAttribArray(0);

        ::glVertexAttribPointer(1
            , sizeof(Vertex::uv_) / sizeof(float)
            , GL_FLOAT
            , GL_FALSE
            , sizeof(Vertex)
            , (void*)offsetof(Vertex, uv_));
        ::glEnableVertexAttribArray(1);

        ::glVertexAttribPointer(2
            , sizeof(Vertex::c_) / sizeof(float)
            , GL_FLOAT
            , GL_FALSE
            , sizeof(Vertex)
            , (void*)offsetof(Vertex, c_));
        ::glEnableVertexAttribArray(2);

        ::glBindBuffer(GL_ARRAY_BUFFER, 0);
        ::glBindVertexArray(0);
    };
    auto clean_up_vertices = [](unsigned int VBO, unsigned int VAO, unsigned int EBO)
    {
        ::glDeleteVertexArrays(1, &VAO);
        ::glDeleteBuffers(1, &VBO);
        ::glDeleteBuffers(1, &EBO);
    };

    unsigned int VBO = 0;
    unsigned int VAO = 0;
    unsigned int EBO = 0;
    upload_vertices(&VBO, &VAO, &EBO);

    ::glEnable(GL_SCISSOR_TEST);
    ::glUseProgram(shader_program_);
    ::glUniform1f(screen_width_ptr_, float(frame_info.screen_size.width));
    ::glUniform1f(screen_height_ptr_, float(frame_info.screen_size.height));
    ::glUniform1i(texture_ptr_, 0);
    ::glActiveTexture(GL_TEXTURE0);
    ::glBindTexture(GL_TEXTURE_2D, white_1x1_.handle());
    ::glBindVertexArray(VAO);

    unsigned bound_texture = white_1x1_.handle();
    auto bind_cmd_texture = [&bound_texture](const DrawCmd& cmd)
    {
        KK_VERIFY(cmd.texture_.is_valid());
        if (cmd.texture_.handle() == bound_texture)
            return;
        ::glBindTexture(GL_TEXTURE_2D, cmd.texture_.handle());
        bound_texture = cmd.texture_.handle();
    };

    auto apply_cmd_clip = [&](const DrawCmd& cmd)
    {
        const kk::Rect clip_rect = ClipRect_Transform(cmd.clip_rect_, cmd.scale_, frame_info.screen_size);
        const kk::Point clip_min = clip_rect.min();
        const kk::Point clip_max = clip_rect.max();
        ::glScissor(clip_min.x
            , frame_info.screen_size.height - clip_max.y // Inverted Y.
            , clip_max.x - clip_min.x
            , clip_max.y - clip_min.y);
    };

    static_assert(sizeof(Index) == 2); // GL_UNSIGNED_SHORT

    for (const DrawCmd& cmd : cmd_list_.draw_list_)
    {
        ::glUniform1f(scale_x_ptr_, cmd.scale_.x);
        ::glUniform1f(scale_y_ptr_, cmd.scale_.y);
        bind_cmd_texture(cmd);
        apply_cmd_clip(cmd);

        void* const indices_offset = (void*)std::uintptr_t(cmd.index_offset_ * sizeof(Index));
        ::glDrawElements(GL_TRIANGLES, cmd.index_count_, GL_UNSIGNED_SHORT, indices_offset);
    }

    ::glDisable(GL_SCISSOR_TEST);

    clean_up_vertices(VBO, VAO, EBO);
}
#endif

#if (KK_RENDER_VULKAN())
// shader.vert
#if (0)
#version 450

layout(location = 0) in vec2 IN_Position;
layout(location = 1) in vec2 IN_UV;
layout(location = 2) in vec4 IN_Color;

layout(location = 0) out vec2 Frag_UV;
layout(location = 1) out vec4 Frag_Color;

layout(push_constant) uniform constants
{
    float ScreenWidth;
    float ScreenHeight;
    float ScaleX;
    float ScaleY;
}
PushConstants;

mat3x3 ortho2d(float screen_width, float screen_height)
{
    float left = 0;
    float right = screen_width;
    float top = screen_height; // Notice flip compared to OpenGL.
    float bottom = 0;

    mat3x3 m = mat3x3(1);
    m[0][0] = 2 / (right - left); // scale
    m[1][1] = 2 / (top - bottom);
    m[2][0] = -(right + left) / (right - left); // move/center
    m[2][1] = -(top + bottom) / (top - bottom);
    return m;
}

mat3x3 scale2d(float scale_x, float scale_y)
{
    mat3x3 m = mat3x3(1);
    m[0][0] = scale_x;
    m[1][1] = scale_y;
    return m;
}

void main()
{
    mat3x3 orhto = ortho2d(PushConstants.ScreenWidth, PushConstants.ScreenHeight);
    mat3x3 scale = scale2d(PushConstants.ScaleX, PushConstants.ScaleY);
    vec3 p = orhto * scale * vec3(IN_Position, 1.0f);
    gl_Position = vec4(p, 1.0f);

    Frag_Color = IN_Color;
    Frag_UV = IN_UV;
}
#endif
// shader.vert, compiled with:
// glslangValidator -V -x -o shader.vert.u32 shader.vert
static const uint32_t kShader_Vertex[] =
{
    // 1011.9.0
    0x07230203,0x00010000,0x0008000a,0x00000086,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x000b000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000006c,0x00000076,0x0000007e,
    0x00000080,0x00000083,0x00000084,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00060005,0x0000000d,0x6874726f,0x2864326f,0x663b3166,0x00003b31,
    0x00060005,0x0000000b,0x65726373,0x775f6e65,0x68746469,0x00000000,0x00060005,0x0000000c,
    0x65726373,0x685f6e65,0x68676965,0x00000074,0x00060005,0x00000011,0x6c616373,0x28643265,
    0x663b3166,0x00003b31,0x00040005,0x0000000f,0x6c616373,0x00785f65,0x00040005,0x00000010,
    0x6c616373,0x00795f65,0x00040005,0x00000013,0x7466656c,0x00000000,0x00040005,0x00000015,
    0x68676972,0x00000074,0x00030005,0x00000017,0x00706f74,0x00040005,0x00000019,0x74746f62,
    0x00006d6f,0x00030005,0x0000001b,0x0000006d,0x00030005,0x00000048,0x0000006d,0x00040005,
    0x00000050,0x7468726f,0x0000006f,0x00050005,0x00000051,0x736e6f63,0x746e6174,0x00000073,
    0x00060006,0x00000051,0x00000000,0x65726353,0x69576e65,0x00687464,0x00070006,0x00000051,
    0x00000001,0x65726353,0x65486e65,0x74686769,0x00000000,0x00050006,0x00000051,0x00000002,
    0x6c616353,0x00005865,0x00050006,0x00000051,0x00000003,0x6c616353,0x00005965,0x00060005,
    0x00000053,0x68737550,0x736e6f43,0x746e6174,0x00000073,0x00040005,0x00000054,0x61726170,
    0x0000006d,0x00040005,0x00000058,0x61726170,0x0000006d,0x00040005,0x0000005c,0x6c616373,
    0x00000065,0x00040005,0x0000005e,0x61726170,0x0000006d,0x00040005,0x00000061,0x61726170,
    0x0000006d,0x00030005,0x00000066,0x00000070,0x00050005,0x0000006c,0x505f4e49,0x7469736f,
    0x006e6f69,0x00060005,0x00000074,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,
    0x00000074,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,0x00000074,0x00000001,
    0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x00000074,0x00000002,0x435f6c67,
    0x4470696c,0x61747369,0x0065636e,0x00070006,0x00000074,0x00000003,0x435f6c67,0x446c6c75,
    0x61747369,0x0065636e,0x00030005,0x00000076,0x00000000,0x00050005,0x0000007e,0x67617246,
    0x6c6f435f,0x0000726f,0x00050005,0x00000080,0x435f4e49,0x726f6c6f,0x00000000,0x00040005,
    0x00000083,0x67617246,0x0056555f,0x00040005,0x00000084,0x555f4e49,0x00000056,0x00050048,
    0x00000051,0x00000000,0x00000023,0x00000000,0x00050048,0x00000051,0x00000001,0x00000023,
    0x00000004,0x00050048,0x00000051,0x00000002,0x00000023,0x00000008,0x00050048,0x00000051,
    0x00000003,0x00000023,0x0000000c,0x00030047,0x00000051,0x00000002,0x00040047,0x0000006c,
    0x0000001e,0x00000000,0x00050048,0x00000074,0x00000000,0x0000000b,0x00000000,0x00050048,
    0x00000074,0x00000001,0x0000000b,0x00000001,0x00050048,0x00000074,0x00000002,0x0000000b,
    0x00000003,0x00050048,0x00000074,0x00000003,0x0000000b,0x00000004,0x00030047,0x00000074,
    0x00000002,0x00040047,0x0000007e,0x0000001e,0x00000001,0x00040047,0x00000080,0x0000001e,
    0x00000002,0x00040047,0x00000083,0x0000001e,0x00000000,0x00040047,0x00000084,0x0000001e,
    0x00000001,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
    0x00000020,0x00040020,0x00000007,0x00000007,0x00000006,0x00040017,0x00000008,0x00000006,
    0x00000003,0x00040018,0x00000009,0x00000008,0x00000003,0x00050021,0x0000000a,0x00000009,
    0x00000007,0x00000007,0x0004002b,0x00000006,0x00000014,0x00000000,0x00040020,0x0000001a,
    0x00000007,0x00000009,0x0004002b,0x00000006,0x0000001c,0x3f800000,0x0006002c,0x00000008,
    0x0000001d,0x0000001c,0x00000014,0x00000014,0x0006002c,0x00000008,0x0000001e,0x00000014,
    0x0000001c,0x00000014,0x0006002c,0x00000008,0x0000001f,0x00000014,0x00000014,0x0000001c,
    0x0006002c,0x00000009,0x00000020,0x0000001d,0x0000001e,0x0000001f,0x00040015,0x00000021,
    0x00000020,0x00000001,0x0004002b,0x00000021,0x00000022,0x00000000,0x0004002b,0x00000006,
    0x00000023,0x40000000,0x00040015,0x00000028,0x00000020,0x00000000,0x0004002b,0x00000028,
    0x00000029,0x00000000,0x0004002b,0x00000021,0x0000002b,0x00000001,0x0004002b,0x00000028,
    0x00000030,0x00000001,0x0004002b,0x00000021,0x00000032,0x00000002,0x0006001e,0x00000051,
    0x00000006,0x00000006,0x00000006,0x00000006,0x00040020,0x00000052,0x00000009,0x00000051,
    0x0004003b,0x00000052,0x00000053,0x00000009,0x00040020,0x00000055,0x00000009,0x00000006,
    0x0004002b,0x00000021,0x0000005d,0x00000003,0x00040020,0x00000065,0x00000007,0x00000008,
    0x00040017,0x0000006a,0x00000006,0x00000002,0x00040020,0x0000006b,0x00000001,0x0000006a,
    0x0004003b,0x0000006b,0x0000006c,0x00000001,0x00040017,0x00000072,0x00000006,0x00000004,
    0x0004001c,0x00000073,0x00000006,0x00000030,0x0006001e,0x00000074,0x00000072,0x00000006,
    0x00000073,0x00000073,0x00040020,0x00000075,0x00000003,0x00000074,0x0004003b,0x00000075,
    0x00000076,0x00000003,0x00040020,0x0000007c,0x00000003,0x00000072,0x0004003b,0x0000007c,
    0x0000007e,0x00000003,0x00040020,0x0000007f,0x00000001,0x00000072,0x0004003b,0x0000007f,
    0x00000080,0x00000001,0x00040020,0x00000082,0x00000003,0x0000006a,0x0004003b,0x00000082,
    0x00000083,0x00000003,0x0004003b,0x0000006b,0x00000084,0x00000001,0x00050036,0x00000002,
    0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x0000001a,0x00000050,
    0x00000007,0x0004003b,0x00000007,0x00000054,0x00000007,0x0004003b,0x00000007,0x00000058,
    0x00000007,0x0004003b,0x0000001a,0x0000005c,0x00000007,0x0004003b,0x00000007,0x0000005e,
    0x00000007,0x0004003b,0x00000007,0x00000061,0x00000007,0x0004003b,0x00000065,0x00000066,
    0x00000007,0x00050041,0x00000055,0x00000056,0x00000053,0x00000022,0x0004003d,0x00000006,
    0x00000057,0x00000056,0x0003003e,0x00000054,0x00000057,0x00050041,0x00000055,0x00000059,
    0x00000053,0x0000002b,0x0004003d,0x00000006,0x0000005a,0x00000059,0x0003003e,0x00000058,
    0x0000005a,0x00060039,0x00000009,0x0000005b,0x0000000d,0x00000054,0x00000058,0x0003003e,
    0x00000050,0x0000005b,0x00050041,0x00000055,0x0000005f,0x00000053,0x00000032,0x0004003d,
    0x00000006,0x00000060,0x0000005f,0x0003003e,0x0000005e,0x00000060,0x00050041,0x00000055,
    0x00000062,0x00000053,0x0000005d,0x0004003d,0x00000006,0x00000063,0x00000062,0x0003003e,
    0x00000061,0x00000063,0x00060039,0x00000009,0x00000064,0x00000011,0x0000005e,0x00000061,
    0x0003003e,0x0000005c,0x00000064,0x0004003d,0x00000009,0x00000067,0x00000050,0x0004003d,
    0x00000009,0x00000068,0x0000005c,0x00050092,0x00000009,0x00000069,0x00000067,0x00000068,
    0x0004003d,0x0000006a,0x0000006d,0x0000006c,0x00050051,0x00000006,0x0000006e,0x0000006d,
    0x00000000,0x00050051,0x00000006,0x0000006f,0x0000006d,0x00000001,0x00060050,0x00000008,
    0x00000070,0x0000006e,0x0000006f,0x0000001c,0x00050091,0x00000008,0x00000071,0x00000069,
    0x00000070,0x0003003e,0x00000066,0x00000071,0x0004003d,0x00000008,0x00000077,0x00000066,
    0x00050051,0x00000006,0x00000078,0x00000077,0x00000000,0x00050051,0x00000006,0x00000079,
    0x00000077,0x00000001,0x00050051,0x00000006,0x0000007a,0x00000077,0x00000002,0x00070050,
    0x00000072,0x0000007b,0x00000078,0x00000079,0x0000007a,0x0000001c,0x00050041,0x0000007c,
    0x0000007d,0x00000076,0x00000022,0x0003003e,0x0000007d,0x0000007b,0x0004003d,0x00000072,
    0x00000081,0x00000080,0x0003003e,0x0000007e,0x00000081,0x0004003d,0x0000006a,0x00000085,
    0x00000084,0x0003003e,0x00000083,0x00000085,0x000100fd,0x00010038,0x00050036,0x00000009,
    0x0000000d,0x00000000,0x0000000a,0x00030037,0x00000007,0x0000000b,0x00030037,0x00000007,
    0x0000000c,0x000200f8,0x0000000e,0x0004003b,0x00000007,0x00000013,0x00000007,0x0004003b,
    0x00000007,0x00000015,0x00000007,0x0004003b,0x00000007,0x00000017,0x00000007,0x0004003b,
    0x00000007,0x00000019,0x00000007,0x0004003b,0x0000001a,0x0000001b,0x00000007,0x0003003e,
    0x00000013,0x00000014,0x0004003d,0x00000006,0x00000016,0x0000000b,0x0003003e,0x00000015,
    0x00000016,0x0004003d,0x00000006,0x00000018,0x0000000c,0x0003003e,0x00000017,0x00000018,
    0x0003003e,0x00000019,0x00000014,0x0003003e,0x0000001b,0x00000020,0x0004003d,0x00000006,
    0x00000024,0x00000015,0x0004003d,0x00000006,0x00000025,0x00000013,0x00050083,0x00000006,
    0x00000026,0x00000024,0x00000025,0x00050088,0x00000006,0x00000027,0x00000023,0x00000026,
    0x00060041,0x00000007,0x0000002a,0x0000001b,0x00000022,0x00000029,0x0003003e,0x0000002a,
    0x00000027,0x0004003d,0x00000006,0x0000002c,0x00000017,0x0004003d,0x00000006,0x0000002d,
    0x00000019,0x00050083,0x00000006,0x0000002e,0x0000002c,0x0000002d,0x00050088,0x00000006,
    0x0000002f,0x00000023,0x0000002e,0x00060041,0x00000007,0x00000031,0x0000001b,0x0000002b,
    0x00000030,0x0003003e,0x00000031,0x0000002f,0x0004003d,0x00000006,0x00000033,0x00000015,
    0x0004003d,0x00000006,0x00000034,0x00000013,0x00050081,0x00000006,0x00000035,0x00000033,
    0x00000034,0x0004007f,0x00000006,0x00000036,0x00000035,0x0004003d,0x00000006,0x00000037,
    0x00000015,0x0004003d,0x00000006,0x00000038,0x00000013,0x00050083,0x00000006,0x00000039,
    0x00000037,0x00000038,0x00050088,0x00000006,0x0000003a,0x00000036,0x00000039,0x00060041,
    0x00000007,0x0000003b,0x0000001b,0x00000032,0x00000029,0x0003003e,0x0000003b,0x0000003a,
    0x0004003d,0x00000006,0x0000003c,0x00000017,0x0004003d,0x00000006,0x0000003d,0x00000019,
    0x00050081,0x00000006,0x0000003e,0x0000003c,0x0000003d,0x0004007f,0x00000006,0x0000003f,
    0x0000003e,0x0004003d,0x00000006,0x00000040,0x00000017,0x0004003d,0x00000006,0x00000041,
    0x00000019,0x00050083,0x00000006,0x00000042,0x00000040,0x00000041,0x00050088,0x00000006,
    0x00000043,0x0000003f,0x00000042,0x00060041,0x00000007,0x00000044,0x0000001b,0x00000032,
    0x00000030,0x0003003e,0x00000044,0x00000043,0x0004003d,0x00000009,0x00000045,0x0000001b,
    0x000200fe,0x00000045,0x00010038,0x00050036,0x00000009,0x00000011,0x00000000,0x0000000a,
    0x00030037,0x00000007,0x0000000f,0x00030037,0x00000007,0x00000010,0x000200f8,0x00000012,
    0x0004003b,0x0000001a,0x00000048,0x00000007,0x0003003e,0x00000048,0x00000020,0x0004003d,
    0x00000006,0x00000049,0x0000000f,0x00060041,0x00000007,0x0000004a,0x00000048,0x00000022,
    0x00000029,0x0003003e,0x0000004a,0x00000049,0x0004003d,0x00000006,0x0000004b,0x00000010,
    0x00060041,0x00000007,0x0000004c,0x00000048,0x0000002b,0x00000030,0x0003003e,0x0000004c,
    0x0000004b,0x0004003d,0x00000009,0x0000004d,0x00000048,0x000200fe,0x0000004d,0x00010038
};

// shader.frag
#if (0)
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec2 Frag_UV;
layout(location = 1) in vec4 Frag_Color;

layout(set = 0, binding = 1) uniform texture2D tex;
layout(set = 0, binding = 2) uniform sampler samp;

layout(location = 0) out vec4 Out_Color;

void main()
{
    Out_Color = Frag_Color * texture(sampler2D(tex, samp), Frag_UV);
}
#endif
// shader.frag, compiled with:
// glslangValidator -V -x -o shader.frag.u32 shader.frag
static const uint32_t kShader_Fragment[] =
{
    // 1011.9.0
    0x07230203,0x00010000,0x0008000a,0x0000001d,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0008000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00000019,
    0x00030010,0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00090004,0x415f4c47,
    0x735f4252,0x72617065,0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00090004,
    0x415f4c47,0x735f4252,0x69646168,0x6c5f676e,0x75676e61,0x5f656761,0x70303234,0x006b6361,
    0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,0x00000009,0x5f74754f,0x6f6c6f43,
    0x00000072,0x00050005,0x0000000b,0x67617246,0x6c6f435f,0x0000726f,0x00030005,0x0000000f,
    0x00786574,0x00040005,0x00000013,0x706d6173,0x00000000,0x00040005,0x00000019,0x67617246,
    0x0056555f,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000b,0x0000001e,
    0x00000001,0x00040047,0x0000000f,0x00000022,0x00000000,0x00040047,0x0000000f,0x00000021,
    0x00000001,0x00040047,0x00000013,0x00000022,0x00000000,0x00040047,0x00000013,0x00000021,
    0x00000002,0x00040047,0x00000019,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,
    0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
    0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,
    0x00000003,0x00040020,0x0000000a,0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,
    0x00000001,0x00090019,0x0000000d,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,
    0x00000001,0x00000000,0x00040020,0x0000000e,0x00000000,0x0000000d,0x0004003b,0x0000000e,
    0x0000000f,0x00000000,0x0002001a,0x00000011,0x00040020,0x00000012,0x00000000,0x00000011,
    0x0004003b,0x00000012,0x00000013,0x00000000,0x0003001b,0x00000015,0x0000000d,0x00040017,
    0x00000017,0x00000006,0x00000002,0x00040020,0x00000018,0x00000001,0x00000017,0x0004003b,
    0x00000018,0x00000019,0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,
    0x000200f8,0x00000005,0x0004003d,0x00000007,0x0000000c,0x0000000b,0x0004003d,0x0000000d,
    0x00000010,0x0000000f,0x0004003d,0x00000011,0x00000014,0x00000013,0x00050056,0x00000015,
    0x00000016,0x00000010,0x00000014,0x0004003d,0x00000017,0x0000001a,0x00000019,0x00050057,
    0x00000007,0x0000001b,0x00000016,0x0000001a,0x00050085,0x00000007,0x0000001c,0x0000000c,
    0x0000001b,0x0003003e,0x00000009,0x0000001c,0x000100fd,0x00010038
};

struct Vertex_PushConstants
{
    float screen_width;
    float screen_height;
    float scale_x;
    float scale_y;
};

static void Vulkan_CreatePipeline(KidsRender::Vulkan_Pipeline& pipeline
    , VkDevice device
    , VkDescriptorSetLayout descriptor_set_layout
    , VkRenderPass render_pass
    , VkSampleCountFlagBits msaa_samples
    , VkShaderModule vertex_module
    , VkShaderModule fragment_module
    , VkPrimitiveTopology topology)
{
    VkPipelineShaderStageCreateInfo vertex_info{};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_info.pNext = nullptr;
    vertex_info.flags = 0;
    vertex_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_info.module = vertex_module;
    vertex_info.pName = "main"; // entrypoint.
    vertex_info.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragment_info{};
    fragment_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_info.pNext = nullptr;
    fragment_info.flags = 0;
    fragment_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_info.module = fragment_module;
    fragment_info.pName = "main"; // entrypoint.
    fragment_info.pSpecializationInfo = nullptr;

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    VkVertexInputBindingDescription binding_description =
    {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };
    VkVertexInputAttributeDescription attribute_descriptions[] =
    {
        VkVertexInputAttributeDescription
        {
            .location = 0,
            .binding = binding_description.binding,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, p_),
        },
        VkVertexInputAttributeDescription
        {
            .location = 1,
            .binding = binding_description.binding,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, uv_),
        },
        VkVertexInputAttributeDescription
        {
            .location = 2,
            .binding = binding_description.binding,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = offsetof(Vertex, c_),
        },
    };
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.pNext = nullptr;
    vertex_input_info.flags = 0;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = uint32_t(std::size(attribute_descriptions));
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.pNext = nullptr;
    input_assembly_info.flags = 0;
    input_assembly_info.topology = topology;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    const VkDynamicState dynamic_states[2] =
    {
        VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT,
        VkDynamicState::VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = uint32_t(std::size(dynamic_states));
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.pNext = nullptr;
    viewport_info.flags = 0;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = nullptr; // dynamic.
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = nullptr; // dynamic.

    VkPipelineRasterizationStateCreateInfo rasterizer_info{};
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.pNext = nullptr;
    rasterizer_info.flags = 0;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer_info.depthBiasEnable = VK_FALSE;
    rasterizer_info.depthBiasConstantFactor = 0.f;
    rasterizer_info.depthBiasClamp = 0.f;
    rasterizer_info.depthBiasSlopeFactor = 0.f;
    rasterizer_info.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo multisampling_info{};
    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.pNext = nullptr;
    multisampling_info.flags = 0;
    multisampling_info.rasterizationSamples = msaa_samples;
    multisampling_info.sampleShadingEnable = VK_FALSE;
    multisampling_info.minSampleShading = 1.f;
    multisampling_info.pSampleMask = nullptr;
    multisampling_info.alphaToCoverageEnable = VK_FALSE;
    multisampling_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.blendEnable = VK_TRUE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_info{};
    color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.pNext = nullptr;
    color_blend_info.flags = 0;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;
    color_blend_info.blendConstants[0] = 0.f;
    color_blend_info.blendConstants[1] = 0.f;
    color_blend_info.blendConstants[2] = 0.f;
    color_blend_info.blendConstants[3] = 0.f;

    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(Vertex_PushConstants);

    KK_VERIFY(descriptor_set_layout);
    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pNext = nullptr;
    pipeline_layout_info.flags = 0;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    KK_VERIFY(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline.layout) == VK_SUCCESS);

    const VkPipelineShaderStageCreateInfo stages[] = {vertex_info, fragment_info};

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = nullptr;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = uint32_t(std::size(stages));
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pTessellationState = nullptr;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisampling_info;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blend_info;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline.layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    KK_VERIFY(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline.pipeline) == VK_SUCCESS);
}

static void Vulkan_KillPipeline(VkDevice device, KidsRender::Vulkan_Pipeline& pipeline)
{
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
    pipeline = {};
}

static void Vulkan_Buffer_Create(VkPhysicalDevice physical_device
    , VkDevice device
    , VkDeviceSize size
    , VkBufferUsageFlags usage
    , VkMemoryPropertyFlags properties
    , VkBuffer& buffer
    , VkDeviceMemory& buffer_memory)
{
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.pNext = nullptr;
    buffer_info.flags = 0;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // buffer_info.queueFamilyIndexCount = 
    // buffer_info.pQueueFamilyIndices = 
    KK_VERIFY(vkCreateBuffer(device, &buffer_info, nullptr, &buffer) == VK_SUCCESS);
    VkMemoryRequirements mem_requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

    VkPhysicalDeviceMemoryProperties mem_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    uint32_t mem_index = uint32_t(-1);
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i)
    {
        if ((mem_requirements.memoryTypeBits & (1 << i))
            && ((mem_properties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            mem_index = i;
            break;
        }
    }
    KK_VERIFY(mem_index != uint32_t(-1));

    VkMemoryAllocateInfo vertex_alloc_info{};
    vertex_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vertex_alloc_info.pNext = nullptr;
    vertex_alloc_info.allocationSize = mem_requirements.size;
    vertex_alloc_info.memoryTypeIndex = mem_index;

    KK_VERIFY(vkAllocateMemory(device, &vertex_alloc_info, nullptr, &buffer_memory) == VK_SUCCESS);
    KK_VERIFY(vkBindBufferMemory(device, buffer, buffer_memory, 0) == VK_SUCCESS);
}

static void Vulkan_Kill_FrameData(KidsRender::Vulkan_Frame& frame, RenderData& render_data)
{
    frame.image_in_use_list_.clear();
    if (frame.descriptor_set_list_.size() > 0)
    {
        KK_VERIFY(vkFreeDescriptorSets(render_data.device
            , render_data.descriptor_pool
            , uint32_t(frame.descriptor_set_list_.size())
            , frame.descriptor_set_list_.data()) == VK_SUCCESS);
    }
    frame.descriptor_set_list_.clear();

    if (frame.vertex_buffer)
    {
        vkDestroyBuffer(render_data.device, frame.vertex_buffer, nullptr);
        frame.vertex_buffer = {};
        vkFreeMemory(render_data.device, frame.vertex_memory, nullptr);
        frame.vertex_memory = {};
    }
    if (frame.index_buffer)
    {
        vkDestroyBuffer(render_data.device, frame.index_buffer, nullptr);
        frame.index_buffer = {};
        vkFreeMemory(render_data.device, frame.index_memory, nullptr);
        frame.index_memory = {};
    }

    for (auto& clean_up : frame.to_flush_)
        clean_up();
    frame.to_flush_.clear();
    std::swap(frame.to_flush_, frame.clean_up_list_);
}

static void Vulkan_Recreate_FrameData(KidsRender::Vulkan_Frame& frame
    , RenderData& render_data
    , const std::span<const Vertex>& vertices
    , const std::span<const Index>& indices)
{
    Vulkan_Kill_FrameData(frame, render_data);
    {
        const std::size_t vertex_size = (sizeof(Vertex) * vertices.size());
        Vulkan_Buffer_Create(render_data.physical_device, render_data.device, vertex_size
            , VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            , frame.vertex_buffer
            , frame.vertex_memory);
        void* vertex_data = nullptr;
        KK_VERIFY(vkMapMemory(render_data.device, frame.vertex_memory, 0, vertex_size, 0, &vertex_data) == VK_SUCCESS);
        memcpy(vertex_data, &vertices[0], vertex_size);
        vkUnmapMemory(render_data.device, frame.vertex_memory);
    }
    {
        const std::size_t index_size = (sizeof(Index) * indices.size());
        Vulkan_Buffer_Create(render_data.physical_device, render_data.device, index_size
            , VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            , frame.index_buffer
            , frame.index_memory);
        void* index_data = nullptr;
        KK_VERIFY(vkMapMemory(render_data.device, frame.index_memory, 0, index_size, 0, &index_data) == VK_SUCCESS);
        memcpy(index_data, &indices[0], index_size);
        vkUnmapMemory(render_data.device, frame.index_memory);
    }
}

static VkDescriptorSet Vulkan_CreateDescriptorSet(const RenderData& render_data
    , VkDescriptorSetLayout descriptor_set_layout
    , VkImageView image_view
    , VkSampler sampler)
{
    VkDescriptorSet descriptor_set{};
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = render_data.descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &descriptor_set_layout;
    KK_VERIFY(vkAllocateDescriptorSets(render_data.device, &alloc_info
        , &descriptor_set) == VK_SUCCESS);

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = image_view;
    image_info.sampler = sampler;

    VkWriteDescriptorSet descriptor_writes[2]{};
    descriptor_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[0].pNext = nullptr;
    descriptor_writes[0].dstSet = descriptor_set;
    descriptor_writes[0].descriptorCount = 1;
    descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptor_writes[0].pImageInfo = &image_info;
    descriptor_writes[0].dstArrayElement = 0;
    descriptor_writes[0].dstBinding = 1;
    descriptor_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptor_writes[1].pNext = nullptr;
    descriptor_writes[1].dstSet = descriptor_set;
    descriptor_writes[1].descriptorCount = 1;
    descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptor_writes[1].pImageInfo = &image_info;
    descriptor_writes[1].dstArrayElement = 0;
    descriptor_writes[1].dstBinding = 2;

    vkUpdateDescriptorSets(render_data.device, 2, descriptor_writes, 0, nullptr);
    return descriptor_set;
}

static VkDescriptorSet Vulkan_Frame_ChooseDescriptoSet_FromCache(KidsRender::Vulkan_Frame& frame
    , RenderData& render_data
    , VkDescriptorSetLayout descriptor_set_layout
    , VkSampler texture_sampler
    , DrawCmd draw_cmd
    , const ImageRef& white_1x1
    , VkDescriptorSet white_descriptor_set)
{
    if (draw_cmd.texture_ == white_1x1)
        return white_descriptor_set;
    KK_VERIFY(frame.descriptor_set_list_.size()
        == frame.image_in_use_list_.size());
    if (!frame.image_in_use_list_.empty())
    {
        if (frame.image_in_use_list_.back() == draw_cmd.texture_)
            return frame.descriptor_set_list_.back();
    }

    frame.image_in_use_list_.push_back(draw_cmd.texture_);
    VkDescriptorSet descriptor_set = Vulkan_CreateDescriptorSet(render_data
        , descriptor_set_layout, draw_cmd.texture_.image_view(), texture_sampler);
    frame.descriptor_set_list_.push_back(descriptor_set);
    return descriptor_set;
}

static VkRect2D Vulkan_Scissor(const DrawCmd& draw_cmd, const kk::Size& screen_size)
{
    const kk::Rect clip_rect = ClipRect_Transform(draw_cmd.clip_rect_, draw_cmd.scale_, screen_size);
    const kk::Point clip_min = clip_rect.min();
    const kk::Point clip_max = clip_rect.max();
    
    VkRect2D scissor{};
    // To make Vulkan validation layer silent.
    // Even tho `offset` is `int32_t`, negative values are not allowed.
    scissor.offset.x = (std::max)(clip_min.x, 0);
    scissor.offset.y = (std::max)(clip_min.y, 0);
    scissor.extent.width  = uint32_t((std::max)(clip_max.x - scissor.offset.x, 0));
    scissor.extent.height = uint32_t((std::max)(clip_max.y - scissor.offset.y, 0));
    return scissor;
}

static void Vulkan_Record_Frame(KidsRender::Vulkan_Frame& frame
    , RenderData& render_data
    , VkDescriptorSetLayout descriptor_set_layout
    , VkSampler texture_sampler
    , DrawCmd draw_cmd
    , const ImageRef& white_1x1
    , VkDescriptorSet white_1x1_descriptor_set
    , KidsRender::Vulkan_Pipeline& pipeline
    , VkCommandBuffer cmd_buffer
    , const kk::Size& screen_size
    , const kk::Vec2f& scale
    )
{
    vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

    Vertex_PushConstants push_constants{};
    push_constants.screen_width = float(screen_size.width);
    push_constants.screen_height = float(screen_size.height);
    push_constants.scale_x = scale.x;
    push_constants.scale_y = scale.y;
    vkCmdPushConstants(cmd_buffer
        , pipeline.layout
        , VK_SHADER_STAGE_VERTEX_BIT
        , 0
        , sizeof(Vertex_PushConstants)
        , &push_constants);

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = float((std::max)(screen_size.width, 1));
    viewport.height = float((std::max)(screen_size.height, 1));
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

    VkRect2D scissor = Vulkan_Scissor(draw_cmd, screen_size);
    vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

    static_assert(sizeof(Index) == 2); // VK_INDEX_TYPE_UINT16.

    VkDeviceSize vertex_offset = 0;
    VkDeviceSize index_offset = 0;
    vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &frame.vertex_buffer, &vertex_offset);
    vkCmdBindIndexBuffer(cmd_buffer, frame.index_buffer, index_offset, VK_INDEX_TYPE_UINT16);

    VkDescriptorSet descriptor_set = Vulkan_Frame_ChooseDescriptoSet_FromCache(
        frame, render_data, descriptor_set_layout, texture_sampler
        , draw_cmd, white_1x1, white_1x1_descriptor_set);

    vkCmdBindDescriptorSets(cmd_buffer
        , VK_PIPELINE_BIND_POINT_GRAPHICS
        , pipeline.layout
        , 0
        , 1
        , &descriptor_set
        , 0
        , nullptr);

    vkCmdDrawIndexed(cmd_buffer
        , uint32_t(draw_cmd.index_count_)
        , 1
        , uint32_t(draw_cmd.index_offset_)
        , 0
        , 0);
}

KidsRender::KidsRender() = default;

KidsRender::~KidsRender() noexcept
{
    if (!render_data_.device)
        return;
    for (Vulkan_Frame& frame : frame_list_)
    {
        Vulkan_Kill_FrameData(frame, render_data_);
        for (auto& clean_up : frame.to_flush_)
            clean_up();
        frame.to_flush_.clear();
    }
    Vulkan_KillPipeline(render_data_.device, triangle_pipeline_);
    vkDestroyDescriptorSetLayout(render_data_.device, descriptor_set_layout_, nullptr);
    vkDestroySampler(render_data_.device, texture_sampler_, nullptr);
    KK_VERIFY(vkFreeDescriptorSets(render_data_.device
        , render_data_.descriptor_pool
        , 1
        , &white_1x1_descriptor_set_) == VK_SUCCESS);
    white_1x1_ = {};
}

static VkSampler Vulkan_CreateTextureSampler(VkDevice device)
{
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.minLod = -1000;
    info.maxLod = 1000;
    info.maxAnisotropy = 1.0f;
    VkSampler sampler{};
    VkResult err = vkCreateSampler(device, &info, nullptr, &sampler);
    KK_VERIFY(err == VK_SUCCESS);
    return sampler;
}

static VkDescriptorSetLayout Vulkan_CreateDescriptorSetLayout(VkDevice device)
{
    VkDescriptorSetLayoutBinding binding[2]{};
    binding[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding[0].descriptorCount = 1;
    binding[0].pImmutableSamplers = nullptr;
    binding[0].binding = 2;

    binding[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    binding[1].descriptorCount = 1;
    binding[1].pImmutableSamplers = nullptr;
    binding[1].binding = 1;

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = uint32_t(std::size(binding));
    info.pBindings = binding;
    VkDescriptorSetLayout descriptor_set_layout{};
    VkResult err = vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptor_set_layout);
    KK_VERIFY(err == VK_SUCCESS);
    return descriptor_set_layout;
}

static VkShaderModule Vulkan_CreateShader(VkDevice device, const void* data, size_t size)
{
    KK_VERIFY(device);

    VkShaderModuleCreateInfo shader_info{};
    shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_info.pNext = nullptr;
    shader_info.flags = 0;
    shader_info.codeSize = size;
    shader_info.pCode = static_cast<const uint32_t*>(data);

    VkShaderModule shader;
    KK_VERIFY(vkCreateShaderModule(device, &shader_info, nullptr/*Allocator*/, &shader) == VK_SUCCESS);
    return shader;
}

/*static*/ void KidsRender::Build(const RenderData& render_data, KidsRender& render)
{
    render.render_data_ = render_data;

    render.texture_sampler_ = Vulkan_CreateTextureSampler(render_data.device);
    render.descriptor_set_layout_ = Vulkan_CreateDescriptorSetLayout(render_data.device);

    VkShaderModule vertex_module = Vulkan_CreateShader(render_data.device
        , kShader_Vertex, sizeof(kShader_Vertex));
    VkShaderModule fragment_module = Vulkan_CreateShader(render_data.device
        , kShader_Fragment, sizeof(kShader_Fragment));

    Vulkan_CreatePipeline(render.triangle_pipeline_
        , render_data.device
        , render.descriptor_set_layout_
        , render_data.render_pass
        , render_data.msaa_samples
        , vertex_module
        , fragment_module
        , VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    vkDestroyShaderModule(render_data.device, vertex_module, nullptr);
    vkDestroyShaderModule(render_data.device, fragment_module, nullptr);

    KK_VERIFY(render_data.image_count > 0);
    render.frame_list_.resize(render_data.image_count);
    render.frame_list_.shrink_to_fit();

    render.white_1x1_ = Texture_White_1x1(render);
    render.white_1x1_descriptor_set_ = Vulkan_CreateDescriptorSet(
        render_data
        , render.descriptor_set_layout_
        , render.white_1x1_.image_view()
        , render.texture_sampler_);
}

void KidsRender::draw(const FrameInfo& frame_info)
{
    if (cmd_list_.draw_list_.empty())
        return;

    KK_VERIFY(frame_info.frame_index < frame_list_.size());
    Vulkan_Frame& current_frame = frame_list_[frame_info.frame_index];

    Vulkan_Recreate_FrameData(current_frame, render_data_
        , cmd_list_.vertex_list_, cmd_list_.index_list_);
    KK_VERIFY(current_frame.image_in_use_list_.empty());

    for (const DrawCmd& cmd : cmd_list_.draw_list_)
    {
        Vulkan_Record_Frame(current_frame
            , render_data_
            , descriptor_set_layout_
            , texture_sampler_
            , cmd
            , white_1x1_
            , white_1x1_descriptor_set_
            , triangle_pipeline_
            , frame_info.command_buffer
            , frame_info.screen_size
            , cmd.scale_);
    }
}

void KidsRender::defer_clean_up(std::function<void ()> f)
{
    KK_VERIFY(render_data_.current_frame_);
    const std::uint32_t index = render_data_.current_frame_();
    KK_VERIFY(index < frame_list_.size());
    Vulkan_Frame& frame = frame_list_[index];
    frame.clean_up_list_.push_back(std::move(f));
}
#endif

} // namespace kr
