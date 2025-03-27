#include "KR_kids_font_fallback.hh"

namespace kr
{

kr::Font_Fallback& Font_Family::select_font(const Font_Style& style)
{
    if (!style.bold && !style.italic)
        return *font;
    else if (style.bold && style.italic)
        return *font_bold_italic;
    else if (style.bold)
        return *font_bold;
    else if (style.italic)
        return *font_italic;
    KK_UNREACHABLE();
}

void Font_Fallback::set_main_font(Font&& main_font)
{
    main_font_ = std::move(main_font);
    metrics_ = main_font_.metrics();
    for (Font& fallback_font : fallback_list_)
        metrics_ = Merge_Metrics(metrics_, fallback_font.metrics());
}

void Font_Fallback::add_font_as_fallback(Font&& new_font)
{
    // #TODO: Assumes `main_font_` is set. Add a check.
    Font& fallback_font = fallback_list_.emplace_back(std::move(new_font));
    metrics_ = Merge_Metrics(metrics_, fallback_font.metrics());
    KK_VERIFY(fallback_font.size() == main_font_.size());
}

GlyphRender Font_Fallback::glyph_render(
    std::uint32_t code_point
    , const Font** source_font /*= nullptr*/)
{
    const GlyphRender& render = main_font_.glyph_render(code_point);
    if (render.glyph_info.glyph_index == 0)
    {
        for (Font& fallback_font : fallback_list_)
        {
            const GlyphRender& fallback_render = fallback_font.glyph_render(code_point);
            if (fallback_render.glyph_info.glyph_index == 0)
                continue;
            if (source_font)
                *source_font = &fallback_font;
            return fallback_render;
        }
    }
    // Placeholder glyph (render) from the main font.
    if (source_font)
        *source_font = &main_font_;
    return render;
}

const GlyphInfo& Font_Fallback::glyph_info(
    std::uint32_t code_point
    , const Font** source_font /*= nullptr*/)
{
    const GlyphInfo& glyph = main_font_.glyph_info(code_point);
    if (glyph.glyph_index == 0)
    {
        for (Font& fallback_font : fallback_list_)
        {
            const GlyphInfo& fallback_glyph = fallback_font.glyph_info(code_point);
            if (fallback_glyph.glyph_index == 0)
                continue;
            if (source_font)
                *source_font = &fallback_font;
            return fallback_glyph;
        }
    }
    // Placeholder glyph from the main font.
    if (source_font)
        *source_font = &main_font_;
    return glyph;
}

} // namespace kr
