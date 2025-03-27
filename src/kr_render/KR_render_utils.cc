#include "KR_render_utils.hh"
#include "KR_kids_render.hh"

namespace kr
{

kr::Render_& Render_::with_width(float width)
{
    global_width_ = width;
    return *this;
}

kr::Render_& Render_::with_color(const kk::Color& color)
{
    global_color_ = color;
    return *this;
}

kr::Render_& Render_::with_scale(const kk::Vec2f& scale)
{
    global_scale_ = scale;
    return *this;
}

kr::Render_& Render_::with_no_scale()
{
    global_scale_.reset();
    return *this;
}

kr::Render_& Render_::with_clip_rect(const ClipRect& clip_rect)
{
    global_clip_rect_ = clip_rect;
    return *this;
}

kr::Render_& Render_::with_cmd_list(CmdList& cmd_list)
{
    global_cmd_list_ = &cmd_list;
    return *this;
}

kr::Render_& Render_::with_radius(float radius)
{
    global_radius_ = radius;
    return *this;
}

kr::Render_& Render_::with_segments_count(int segments_count)
{
    global_segments_count_ = segments_count;
    return *this;
}

float Render_::width() const
{
    return global_width_.value_or(1.f);
}

kk::Color Render_::color() const
{
    return global_color_.value_or(kk::Color_White());
}

kk::Vec2f Render_::scale() const
{
    return global_scale_.value_or(kk::Vec2f{1.f, 1.f});
}

kr::ClipRect Render_::clip_rect() const
{
    return global_clip_rect_.value_or(ClipRect{});
}

kr::CmdList* Render_::cmd_list() const
{
    return global_cmd_list_;
}

float Render_::radius() const
{
    return global_radius_.value_or(1.f);
}

int Render_::segments_count() const
{
    return global_segments_count_.value_or(-1);
}

kk::Vec2f Render_::uv_min() const
{
    return global_uv_min_.value_or(kk::Vec2f{0.f, 0.f});
}

kk::Vec2f Render_::uv_max() const
{
    return global_uv_max_.value_or(kk::Vec2f{1.f, 1.f});
}

kr::Render_& Render_::line(const kk::Point2f& p1, const kk::Point2f& p2)
{
    r_->line(p1, p2, color(), width(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::line(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Color& color_)
{
    r_->line(p1, p2, color_, width(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::triangle(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3)
{
    r_->triangle(p1, p2, p3, color(), width(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::triangle(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3, const kk::Color& color_)
{
    r_->triangle(p1, p2, p3, color_, width(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::triangle_fill(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3)
{
    r_->triangle_fill(p1, p2, p3, color(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::triangle_fill(const kk::Point2f& p1, const kk::Point2f& p2, const kk::Point2f& p3, const kk::Color& color_)
{
    r_->triangle_fill(p1, p2, p3, color_, scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::rect(const kk::Point2f& p_min, const kk::Point2f& p_max)
{
    r_->rect(p_min, p_max, color(), width(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::rect(const kk::Point2f& p_min, const kk::Point2f& p_max, const kk::Color& color_)
{
    r_->rect(p_min, p_max, color_, width(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::rect(const kk::Rect2f& r)
{
    return rect(r.min(), r.max(), color());
}

kr::Render_& Render_::rect(const kk::Rect2f& r, const kk::Color& color_)
{
    return rect(r.min(), r.max(), color_);
}

kr::Render_& Render_::rect_fill(const kk::Point2f& p_min, const kk::Point2f& p_max)
{
    r_->rect_fill(p_min, p_max, color(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::rect_fill(const kk::Point2f& p_min, const kk::Point2f& p_max, const kk::Color& color_)
{
    r_->rect_fill(p_min, p_max, color_, scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::rect_fill(const kk::Rect2f& r)
{
    return rect_fill(r.min(), r.max(), color());
}

kr::Render_& Render_::rect_fill(const kk::Rect2f& r, const kk::Color& color_)
{
    return rect_fill(r.min(), r.max(), color_);
}

kr::Render_& Render_::circle(const kk::Point2f& p_center)
{
    r_->circle(p_center, radius(), color(), width(), scale(), segments_count(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::circle(const kk::Point2f& p_center, float radius_)
{
    r_->circle(p_center, radius_, color(), width(), scale(), segments_count(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::circle_fill(const kk::Point2f& p_center)
{
    r_->circle_fill(p_center, radius(), color(), scale(), segments_count(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::circle_fill(const kk::Point2f& p_center, float radius_)
{
    r_->circle_fill(p_center, radius_, color(), scale(), segments_count(), clip_rect(), cmd_list());
    return *this;
}

Render_& Render_::image(const ImageRef& image_
    , const kk::Point2f& p_min // = kk::Point2f{0, 0}
    , const kk::Point2f& p_max // = kk::Point2f_Invalid() // Use image's size.
    )
{
    if (!image_.is_valid())
        return *this;
    r_->image(image_, p_min, p_max, uv_min(), uv_max(), color(), scale(), clip_rect(), cmd_list());
    return *this;
}

kr::Render_& Render_::image(const ImageRef& image_, const kk::Rect2f& rect)
{
    return image(image_, rect.min(), rect.max());
}

} // namespace kr
