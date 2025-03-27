#pragma once
#include "KR_kids_font.hh"

namespace kr
{

class Font_Fallback;

struct Font_Style
{
    bool bold = false;
    bool italic = false;
};

struct Font_Family
{
    Font_Fallback* font = nullptr;
    Font_Fallback* font_bold = nullptr;
    Font_Fallback* font_italic = nullptr;
    Font_Fallback* font_bold_italic = nullptr;

    Font_Fallback& select_font(const Font_Style& style);
};

class Font_Fallback
{
public:
    Font main_font_;
    std::vector<Font> fallback_list_;
    Font_Metrics metrics_;

    void set_main_font(Font&& main_font);
    void add_font_as_fallback(Font&& fallback_font);

    const GlyphInfo& glyph_info(std::uint32_t code_point
        , const Font** source_font = nullptr);

    GlyphRender glyph_render(std::uint32_t code_point
        , const Font** source_font = nullptr);

    const Font_Metrics& metrics() const { return metrics_; }
};

} // namespace kr
