#include "KR_text_shaper.hh"
#include "KR_kids_font_fallback.hh"

namespace kr
{

static const kk::Point kBaselineStart{0, 0};

bool Text_Markup::has_underline() const
{
    return (underline_color_.a > 0);
}

bool Text_Markup::has_overline() const
{
    return (overline_color_.a > 0);
}

bool Text_Markup::has_strikethrough() const
{
    return (strikethrough_color_.a > 0);
}

bool Text_Markup::has_background() const
{
    return (background_color_.a > 0);
}

static int LineToBaselinePenOffset(const Font_Metrics& m)
{
#if (0)
    // `max_descent_` is negative.
    const int linegap = m.line_height_px - (m.ascent_px - m.descent_px);
    const int gap_bottom = linegap / 2;
    const int gap_top = (linegap - gap_bottom);
    // Should be same as `ascent_px` or `y_max_px`. But looks a bit better?
    // `ascent_px` looks too small, `y_max_px` looks too big.
    // Probably I'm doing this wrong.
    const int max_height = (gap_top + m.ascent_px);
#elif (0)
    const int max_height = (m.ascent_px + m.y_max_px) / 2;
#else
    // Seems like this is what is used in DirectWrite.
    // Compared to what is rendered in WordPad...
    int descent_px = -1 * m.descent_px; // since it's negative
    descent_px += 1; // Not sure about this.
    const int max_height = (m.line_height_px - descent_px);
#endif
    return (max_height * -1);
}

kk::Point Text_Metrics::baseline_offset() const
{
    return (kBaselineStart - (rect.min)());
}

kk::Size Text_Metrics::size() const
{
    return rect.size();
}

static kk::Rect Merge_AABB(const kk::Rect& lhs, const kk::Rect& rhs)
{
    const int min_x = (std::min)(lhs.x, rhs.x);
    const int min_y = (std::min)(lhs.y, rhs.y);
    const int max_x = (std::max)(lhs.x + lhs.width, rhs.x + rhs.width);
    const int max_y = (std::max)(lhs.y + lhs.height, rhs.y + rhs.height);

    kk::Rect r;
    r.x = min_x;
    r.y = min_y;
    r.width = (max_x - min_x);
    r.height = (max_y - min_y);
    KK_VERIFY(r.width >= 0);
    KK_VERIFY(r.height >= 0);
    return r;
}

kk::Point Text_Shaper::line_pen(int line_index, const Font_Metrics& font_metrics) const
{
    kk::Point pen = kBaselineStart;
    if (line_index >= 1)
    {
        KK_VERIFY(line_index <= int(line_list_.size()));
        const Text_ShaperLine& prev_line = line_list_[std::size_t(line_index - 1)];
        pen.y = prev_line.pen_.y + font_metrics.line_height_px;
    }
    return pen;
}

void Text_Shaper::line_setup_new(const Font_Fallback& font_fallback)
{
    Text_ShaperLine line;
    line.metrics_ = font_fallback.metrics();
    line.pen_ = line_pen(int(line_list_.size()), line.metrics_);
    line.min_aabb_ = kk::Rect::From(line.pen_, kk::Size{});
    line.last_glyph_index_ = 0;
    line.last_glyph_font_ = nullptr;
    line.text_offset_start_ = text_bytes_consumed_;
    line.text_offset_end_ = line.text_offset_start_;
    line_list_.push_back(std::move(line));
}

void Text_Shaper::line_move_to_new(const Font_Fallback& font_fallback)
{
    KK_VERIFY(line_list_.size() > 0);
    Text_ShaperLine& last_line = line_list_.back();
    last_line.text_offset_end_ = text_bytes_consumed_;
    this->height_by_lines_ += last_line.metrics_.line_height_px;
    this->min_aabb_ = Merge_AABB(min_aabb_, last_line.min_aabb_);
    this->metrics_ = Merge_Metrics(metrics_, last_line.metrics_);
    line_setup_new(font_fallback);
}

bool Text_Shaper::line_is_wrap_required(const Text_ShaperLine& line) const
{
    if (wrap_width_ < 0)
        return false;
    const int width = (line.pen_ - kBaselineStart).x;
    return (width >= wrap_width_);
}

void Text_Shaper::text_add(const Text_UTF8& text_utf8, const Text_Markup& markup)
{
    KK_VERIFY(!finished_);

    // Empty text adds initial line anyway.
    // Otherwise, "metrics" does not make much sense.
    if (line_list_.empty())
        line_setup_new(*markup.font_fallback_);

    UTF8_IterateLines(text_utf8
        , [&](const kr::LineCodepointMeta& meta)
    {
        if (meta.codepoint_part.bytes_count() == 0)
            return;
        text_add_codepoint(meta.codepoint
            , markup
            , meta.codepoint_part.bytes_count());
    }
        , use_crlf_);
}

void Text_Shaper::text_add_utf8(const char* utf8, const Text_Markup& markup)
{
    text_add(Text_UTF8{utf8}, markup);
}

void Text_Shaper::text_add_codepoint(std::uint32_t codepoint
    , const Text_Markup& markup
    , int text_bytes_consumed)
{
    KK_VERIFY(!finished_);
    KK_VERIFY(line_list_.size() > 0);
    Font_Fallback& font_fallback = *markup.font_fallback_;
    text_bytes_consumed_ += text_bytes_consumed;

    if (codepoint == '\n')
    {
        line_move_to_new(font_fallback);
        return;
    }

    const Font_Metrics& font_metrics = font_fallback.metrics();
    Text_ShaperLine& line = line_list_.back();

    // We record with no clipping, added later, when rendering command lists.
    const ClipRect no_clip;
    // Source font (when fallback list is used) needed for kerning support.
    const Font* source_font = nullptr;
    kk::Point kerning_delta;

    const GlyphRender& glyph_render = font_fallback.glyph_render(codepoint, &source_font);
    const GlyphInfo& glyph_info = glyph_render.glyph_info;

    if (!disable_kerning_)
    {
        if (source_font == line.last_glyph_font_)
        {
            kerning_delta = source_font->kerning_delta(
                line.last_glyph_index_, glyph_info.glyph_index);
        }
    }

    kk::Rect glyph_rect;
    glyph_rect.x = (line.pen_.x + kerning_delta.x + glyph_info.bitmap_delta.x);
    glyph_rect.y = (line.pen_.y + kerning_delta.y - glyph_info.bitmap_delta.y);
    glyph_rect.width = int(glyph_info.size.x);
    glyph_rect.height = int(glyph_info.size.y);
    
    if (render_ && markup.has_background())
    { // BACKGROUND
        const kk::Point prev_line_pen = line_pen(int(line_list_.size()) - 1, font_metrics);
        kk::Rect2f rect_background;
        rect_background.x = float(line.pen_.x + kerning_delta.x);
        rect_background.width = float(glyph_render.glyph_info.advance.x);
        rect_background.y = float(prev_line_pen.y + LineToBaselinePenOffset(font_metrics));
        rect_background.height = float(font_metrics.line_height_px);
        render_->rect_fill(
            (rect_background.min)()
            , (rect_background.max)()
            , markup.background_color_
            , kk::Vec2f{1.f, 1.f}
            , no_clip
            , &background_cmd_list_);
    }
    if (render_ && markup.has_underline())
    { // UNDERLINE
        KK_VERIFY(font_metrics.underline_offset_px <= 0);
        float underline_offset = (-1.f * font_metrics.underline_offset_px);
        underline_offset -= font_metrics.underline_thickness_px / 2.f;
        kk::Point2f p_min = Vec2f_From2i(line.pen_);
        p_min.x += kerning_delta.x;
        p_min.y += underline_offset;
        kk::Point2f p_max = p_min;
        p_max.y += font_metrics.underline_thickness_px;
        p_max.x += glyph_render.glyph_info.advance.x;
        render_->rect_fill(
            p_min
            , p_max
            , markup.underline_color_
            , kk::Vec2f{1.f, 1.f}
            , no_clip
            , &background_cmd_list_);
    }
    if (render_ && markup.has_overline())
    { // OVERLINE
        float overline_offset = float(font_metrics.ascent_px);
        overline_offset -= (font_metrics.underline_thickness_px / 2.f);
        kk::Point2f p_min = Vec2f_From2i(line.pen_);
        p_min.x += kerning_delta.x;
        p_min.y -= overline_offset;
        kk::Point2f p_max = p_min;
        p_max.y += font_metrics.underline_thickness_px;
        p_max.x += glyph_render.glyph_info.advance.x;
        render_->rect_fill(
            p_min
            , p_max
            , markup.overline_color_
            , kk::Vec2f{1.f, 1.f}
            , no_clip
            , &background_cmd_list_);
    }

    if (render_)
    { // GLYPH itself
        const kk::Point2f glyph_p_min = Vec2f_From2i(glyph_rect.min());
        const kk::Point2f glyph_p_max = Vec2f_From2i(glyph_rect.max());
        render_->image(glyph_render.texture
            , glyph_p_min
            , glyph_p_max
            , glyph_render.uv.min()
            , glyph_render.uv.max()
            , markup.color_
            , kk::Vec2f{1.f, 1.f} // scale
            , no_clip
            , &glyph_cmd_list_);
    }

    if (render_ && markup.has_strikethrough())
    { // STRIKETHROUGH
        float strikethrough_offset = (0.33f * font_metrics.ascent_px);
        strikethrough_offset += (font_metrics.underline_thickness_px / 2.f);
        kk::Point2f p_min;
        p_min.x = float(line.pen_.x);
        p_min.y = float(line.pen_.y);
        p_min.x += kerning_delta.x;
        p_min.y -= strikethrough_offset;
        kk::Point2f p_max = p_min;
        p_max.y += font_metrics.underline_thickness_px;
        p_max.x += glyph_render.glyph_info.advance.x;
        render_->rect_fill(
            p_min
            , p_max
            , markup.strikethrough_color_
            , kk::Vec2f{1.f, 1.f}
            , no_clip
            , &foreground_cmd_list_);
    }

    line.metrics_ = Merge_Metrics(line.metrics_, font_metrics);
    line.min_aabb_ = Merge_AABB(line.min_aabb_, glyph_rect);
    line.last_glyph_index_ = glyph_info.glyph_index;
    line.last_glyph_font_ = source_font;
    line.pen_.x += kerning_delta.x;
    line.pen_ += kk::Point{int(glyph_info.advance.x), int(glyph_info.advance.y)};

    if (line_is_wrap_required(line))
        line_move_to_new(font_fallback);
}

void Text_Shaper::finish()
{
    KK_VERIFY(!finished_);
    KK_VERIFY(line_list_.size() > 0);
    finished_ = true;
    Text_ShaperLine& last_line = line_list_.back();
    last_line.text_offset_end_ = text_bytes_consumed_;
    this->height_by_lines_ += last_line.metrics_.line_height_px;
    this->min_aabb_ = Merge_AABB(min_aabb_, last_line.min_aabb_);
    this->metrics_ = Merge_Metrics(metrics_, last_line.metrics_);
}

kk::Point Text_Shaper::start_point() const
{
    return kBaselineStart;
}

Text_Metrics Text_Shaper::metrics_min() const
{
    KK_VERIFY(finished_);
    Text_Metrics m;
    m.rect = min_aabb_;
    return m;
}

Text_Metrics Text_Shaper::metrics() const
{
    KK_VERIFY(finished_);
    KK_VERIFY(line_list_.size() > 0);
    const Text_ShaperLine& top_line = line_list_.front();
    const Font_Metrics& font_metrics = top_line.metrics_;
    const kk::Point pen = line_pen(0, font_metrics);
    Text_Metrics m;
    m.rect = min_aabb_;
    m.rect.y = pen.y + LineToBaselinePenOffset(font_metrics);
    m.rect.height = height_by_lines_;
    return m;
}

void Text_Shaper::draw(const kk::Point2f& p_baseline_offset /*= {0.f, 0.f}*/
    , const ClipRect& clip_rect /*= {}*/) const
{
    KK_VERIFY(render_);
    KK_VERIFY(finished_);
    kk::Point2f translate_by = p_baseline_offset;
    render_->merge_cmd_lists(background_cmd_list_, &translate_by, &clip_rect);
    render_->merge_cmd_lists(glyph_cmd_list_, &translate_by, &clip_rect);
    render_->merge_cmd_lists(foreground_cmd_list_, &translate_by, &clip_rect);
}

} // namespace kr
