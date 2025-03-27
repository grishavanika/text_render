#pragma once
#include "KR_kids_api_fwd.hh"
#include "KR_kids_config.hh"
#include "KS_basic_math.hh"
#include <optional>

namespace kr
{

struct Render_
{
    KidsRender* r_ = nullptr;
    Render_(KidsRender& r) : r_{&r} {}

    std::optional<float> global_width_;
    std::optional<kk::Color> global_color_;
    std::optional<kk::Vec2f> global_scale_;
    std::optional<ClipRect> global_clip_rect_;
    CmdList* global_cmd_list_ = nullptr;
    std::optional<float> global_radius_;
    std::optional<int> global_segments_count_;
    std::optional<kk::Vec2f> global_uv_min_;
    std::optional<kk::Vec2f> global_uv_max_;

    Render_& with_width(float width);
    Render_& with_color(const kk::Color& color);
    Render_& with_scale(const kk::Vec2f& scale);
    Render_& with_no_scale();
    Render_& with_clip_rect(const ClipRect& clip_rect);
    Render_& with_cmd_list(CmdList& cmd_list);
    Render_& with_radius(float radius);
    Render_& with_segments_count(int segments_count);

    float width() const;
    kk::Color color() const;
    kk::Vec2f scale() const;
    ClipRect clip_rect() const;
    CmdList* cmd_list() const;
    float radius() const;
    int segments_count() const;
    kk::Vec2f uv_min() const;
    kk::Vec2f uv_max() const;

    Render_& line(const kk::Point2f& p1, const kk::Point2f& p2);
    Render_& line(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Color& color_);

    Render_& triangle(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3);
    Render_& triangle(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3, const kk::Color& color_);
    Render_& triangle_fill(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3);
    Render_& triangle_fill(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3, const kk::Color& color_);

    Render_& rect(const kk::Point2f& p_min, const kk::Point2f& p_max);
    Render_& rect(const kk::Point2f& p_min, const kk::Point2f& p_max, const kk::Color& color_);
    Render_& rect(const kk::Rect2f& r);
    Render_& rect(const kk::Rect2f& r, const kk::Color& color_);
    Render_& rect_fill(const kk::Point2f& p_min, const kk::Point2f& p_max);
    Render_& rect_fill(const kk::Point2f& p_min, const kk::Point2f& p_max, const kk::Color& color_);
    Render_& rect_fill(const kk::Rect2f& r);
    Render_& rect_fill(const kk::Rect2f& r, const kk::Color& color_);

    Render_& circle(const kk::Point2f& p_center);
    Render_& circle(const kk::Point2f& p_center, float radius_);
    Render_& circle_fill(const kk::Point2f& p_center);
    Render_& circle_fill(const kk::Point2f& p_center, float radius_);

    Render_& image(const ImageRef& image_
        , const kk::Point2f& p_min = kk::Point2f{0, 0}
        , const kk::Point2f& p_max = kk::Point2f_Invalid());
    Render_& image(const ImageRef& image_, const kk::Rect2f& rect);
};

} // namespace kr

