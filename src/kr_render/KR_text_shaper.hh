#pragma once
#include "KR_kids_UTF8_text.hh"
#include "KS_basic_math.hh"
#include "KR_kids_render.hh"
#include "KR_kids_font.hh"

namespace kr
{

class Font_Fallback;

struct Text_Markup
{
    Font_Fallback* font_fallback_   = nullptr;
    kk::Color color_                = {0xff, 0xff, 0xff, 0xff};
    kk::Color underline_color_      = {0x00, 0x00, 0x00, 0x00};
    kk::Color overline_color_       = {0x00, 0x00, 0x00, 0x00};
    kk::Color strikethrough_color_  = {0x00, 0x00, 0x00, 0x00};
    kk::Color background_color_     = {0x00, 0x00, 0x00, 0x00};

    bool has_underline() const;
    bool has_overline() const;
    bool has_strikethrough() const;
    bool has_background() const;
};

// Everything below starts with baseline at pen (origin) position {0, 0} (see `kBaselineStart`).
struct Text_ShaperLine
{
    // Current pen position.
    kk::Point pen_{};
    kk::Rect min_aabb_;
    Font_Metrics metrics_;
    // #TODO: Bad idea to store a pointer to 
    // a font that is set by _temporary_ Text_Markup call.
    const Font* last_glyph_font_ = nullptr;
    GlyphIndex last_glyph_index_ = 0;
    int text_offset_start_ = 0;
    int text_offset_end_ = 0;
};

struct Text_Metrics
{
    kk::Rect rect;
    kk::Point baseline_offset() const;
    kk::Size size() const;
};

struct Text_Shaper
{
    // Input.
    // 
    // When nullptr, only stats are collected, no actual drawing happens.
    KidsRender* render_ = nullptr;
    int wrap_width_ = -1;
    bool use_crlf_ = false;
    bool disable_kerning_ = false;

    // Output.
    CmdList background_cmd_list_;
    CmdList glyph_cmd_list_;
    CmdList foreground_cmd_list_;
    std::vector<Text_ShaperLine> line_list_;
    kk::Rect min_aabb_;
    Font_Metrics metrics_;
    int height_by_lines_ = 0;
    bool finished_ = false;
    int text_bytes_consumed_ = 0;

    // State change.
    void text_add_utf8(const char* utf8, const Text_Markup& markup);
    void text_add(const Text_UTF8& text_utf8, const Text_Markup& markup);
    void finish();

    // Helper to render prepared `cmd_list_`.
    void draw(const kk::Point2f& p_baseline_offset = {0.f, 0.f}
        , const ClipRect& clip_rect = {}) const;

    // Query.
    kk::Point start_point() const;
    Text_Metrics metrics_min() const;
    Text_Metrics metrics() const;

private:
    void text_add_codepoint(std::uint32_t codepoint, const Text_Markup& markup, int text_bytes_consumed);

    void line_setup_new(const Font_Fallback& font_fallback);
    void line_move_to_new(const Font_Fallback& font_fallback);
    bool line_is_wrap_required(const Text_ShaperLine& line) const;
    kk::Point line_pen(int line_index, const Font_Metrics& font_metrics) const;
};

} // namespace kr
