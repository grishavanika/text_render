#pragma once
#include "KR_kids_config.hh"
#include "KR_kids_api_fwd.hh"
#include "KR_kids_image.hh"

#include <vector>
#include <functional>

// FreeType 2.0 Tutorial:
// https://stuff.mit.edu/afs/athena/astaff/source/src-9.0/third/freetype/docs/tutorial/step2.html

namespace kr
{

struct Font_Page;
// `Font` does not depend explicitly on `KidsRender`.
// We only need to create an image/atlas for a Font_Page.
using ImageFactory_RGBA = std::function<ImageRef (int width, int height, const void* data)>;

using GlyphIndex = unsigned;

struct GlyphInfo
{
    kk::Vec2u size;
    kk::Vec2i bitmap_delta;
    kk::Vec2u advance;
    GlyphIndex glyph_index = 0; // 0 = undefined
};

struct GlyphRender
{
    GlyphInfo glyph_info;
    ImageRef texture;
    kk::Rect2f uv;
};

struct Font_Metrics
{
    int line_height_px = 0;
    int ascent_px = 0;
    int descent_px = 0;
    int y_max_px = 0;
    int underline_offset_px = 0;
    int underline_thickness_px = 0;
    int max_advance_px = 0;
};

struct Font_Size
{
    static const int DPI_Default = 96;

    static Font_Size Pixels(int size_px, int DPI = DPI_Default);
    static Font_Size Points(float size_pt, int DPI = DPI_Default);

    // Assumed to be at `DPI_Default` (96) DPI **always**.
    // Converted back to points and scaled by `DPI`.
    int size_px = -1;
    // pt, in points (as in "a 12 point font").
    // Scaled by `DPI`.
    float size_pt = -1;
    int DPI = -1;

    float pts() const;
    float pxs() const;
    kk::Vec2f dpi_scale() const;
};

bool operator==(const Font_Size& lhs, const Font_Size& rhs);
bool operator!=(const Font_Size& lhs, const Font_Size& rhs);

Font_Metrics Merge_Metrics(const Font_Metrics& lhs, const Font_Metrics& rhs);
Font_Size Merge_Sizes(const Font_Size& lhs, const Font_Size& rhs);

struct Font_FreeTypeLibrary
{
    explicit Font_FreeTypeLibrary();
    ~Font_FreeTypeLibrary() noexcept;
    Font_FreeTypeLibrary(const Font_FreeTypeLibrary&) = delete;
    Font_FreeTypeLibrary& operator=(const Font_FreeTypeLibrary&) = delete;
    Font_FreeTypeLibrary(Font_FreeTypeLibrary&&) noexcept;
    Font_FreeTypeLibrary& operator=(Font_FreeTypeLibrary&&) noexcept;
    void* ft_library_ = nullptr;
};

// Represents Font **Face**.
// https://freetype.org/freetype2/docs/glyphs/glyphs-1.html
// "The single term `font` is nearly always used in ambiguous ways
//  to refer to either a given family or given face,
//  depending on the context."
// 
class Font
{
public:
    Font() noexcept;
    ~Font() noexcept;
    Font(const Font&) noexcept = delete;
    Font& operator=(const Font&) noexcept = delete;
    Font(Font&&) noexcept;
    Font& operator=(Font&&) noexcept;

public:
    static Font FromFile(Font_FreeTypeLibrary& font_lib
        , const ImageFactory_RGBA& image_factory
        , const char* ttf_file_path
        , const Font_Size& size);

    const GlyphInfo& glyph_info(std::uint32_t code_point);
    GlyphRender glyph_render(std::uint32_t code_point);
    kk::Point kerning_delta(GlyphIndex left_glyph, GlyphIndex right_glyph) const;

    void set_size(const Font_Size& new_size);
    const Font_Size& size() const { return size_; }

    const Font_Metrics& metrics() const { return metrics_; }

private:
    Font_Page& get_or_create_font_page(std::uint32_t code_point);

private:
    void* ft_face_ = nullptr;
    ImageFactory_RGBA image_factory_;
    std::vector<Font_Page> page_list_;
    Font_Size size_;
    Font_Metrics metrics_;
    bool has_kerning_ = false;
};

// Utility to make `ImageFactory_RGBA` out of `render`.
Font Font_FromFile(Font_FreeTypeLibrary& font_init
    , KidsRender& render
    , const char* ttf_file_path
    , const Font_Size& size);

} // namespace kr
