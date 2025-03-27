#include "KR_kids_font.hh"
#include "KR_kids_image.hh"
#include "KR_kids_render.hh"
#include "KR_kids_UTF8_text.hh"

#include <math.h>

#include <utility>

#include <ft2build.h>
#include FT_FREETYPE_H

// From SDL_ttf: Handy routines for converting from fixed point
#define FT_CEIL(X)  (((X + 63) & -64) / 64)

#if (0)
inline float convert_F26Dot6_to_float(FT_F26Dot6 value)
{
    return ((float)value) / 64.0f;
}
#endif
inline FT_F26Dot6 convert_float_to_F26Dot6(float value)
{
    return (FT_F26Dot6)(value * 64.0f);
}
inline float pixels_to_points(int pixels, int at_dpi)
{
    // https://freetype.org/freetype2/docs/glyphs/glyphs-2.html
    // pixel_size = point_size * resolution / 72
    KK_VERIFY(pixels >= 0);
    KK_VERIFY(at_dpi > 0);
    const float pt_size = (72 * pixels) / float(at_dpi);
    return pt_size;
}
inline float font_size_points(const kr::Font_Size& size)
{
    if (size.size_pt >= 0)
        return size.size_pt;
    // Now, pixels are always set in a "default" resolution (96 DPI).
    // We convert back to points and then user-set/current DPI applied
    // to have "scale" effect.
    return pixels_to_points(size.size_px, kr::Font_Size::DPI_Default);
}

namespace kr
{

/*static*/ Font_Size Font_Size::Pixels(int size_px, int DPI /*= DPI_Default*/)
{
    return Font_Size{.size_px = size_px, .size_pt = -1, .DPI = DPI};
}
/*static*/ Font_Size Font_Size::Points(float size_pt, int DPI /*= DPI_Default*/)
{
    return Font_Size{.size_px = -1, .size_pt = size_pt, .DPI = DPI};
}

float Font_Size::pts() const
{
    return font_size_points(*this);
}

float Font_Size::pxs() const
{
    KK_VERIFY(DPI > 0);
    return (pts() * DPI / 72.f);
}

kk::Vec2f Font_Size::dpi_scale() const
{
    KK_VERIFY(DPI > 0);
    const float scale = DPI / float(DPI_Default);
    return {scale, scale};
}

bool operator==(const Font_Size& lhs, const Font_Size& rhs)
{
    return (lhs.size_pt == rhs.size_pt)
        && (lhs.size_px == rhs.size_px)
        && (lhs.DPI == rhs.DPI);
}

bool operator!=(const Font_Size& lhs, const Font_Size& rhs)
{
    return !(lhs == rhs);
}

/*explicit*/ Font_FreeTypeLibrary::Font_FreeTypeLibrary()
    : ft_library_(nullptr)
{
    FT_Library lib{};
    KK_VERIFY(!FT_Init_FreeType(&lib));
    ft_library_ = lib;
}
Font_FreeTypeLibrary::~Font_FreeTypeLibrary() noexcept
{
    if (!ft_library_)
        return;
    KK_VERIFY(!FT_Done_FreeType(static_cast<FT_Library>(ft_library_)));
    ft_library_ = nullptr;
}

Font_FreeTypeLibrary::Font_FreeTypeLibrary(Font_FreeTypeLibrary&& rhs) noexcept
    : ft_library_(std::exchange(rhs.ft_library_, nullptr))
{
}

Font_FreeTypeLibrary& Font_FreeTypeLibrary::operator=(Font_FreeTypeLibrary&& rhs) noexcept
{
    if (this != &rhs)
    {
        this->~Font_FreeTypeLibrary();
        ft_library_ = std::exchange(rhs.ft_library_, nullptr);
    }
    return *this;
}

/*static*/ Font Font::FromFile(Font_FreeTypeLibrary& font_lib
    , const ImageFactory_RGBA& image_factory
    , const char* ttf_file_path
    , const Font_Size& size)
{
    FT_Library lib = static_cast<FT_Library>(font_lib.ft_library_);
    KK_VERIFY(lib);
    FT_Face face{};
    KK_VERIFY(!FT_New_Face(lib, ttf_file_path, 0, &face));
    KK_VERIFY(!FT_Select_Charmap(face, FT_ENCODING_UNICODE));
    // We use metrics that work only for "scalable" fonts.
    KK_VERIFY(FT_IS_SCALABLE(face));

    Font font;
    font.image_factory_ = image_factory;
    font.ft_face_ = face;
    font.has_kerning_ = FT_HAS_KERNING(face);
    font.set_size(size);

    return font;
}

static Font_Metrics Font_QueryMetrics(FT_Face face, const Font_Size& size)
{
#if (0)
    KK_VERIFY(!FT_Set_Pixel_Sizes(face, 0, font_size_px));
#else
    KK_VERIFY(size.DPI > 0);
    const FT_UInt vert_resolution = size.DPI;
    const FT_UInt horz_resolution = 0; // same as vertical
    const FT_F26Dot6 char_width = 0; // same as height
    const float pt_size = font_size_points(size);
    const FT_F26Dot6 char_height = convert_float_to_F26Dot6(pt_size);
    KK_VERIFY(!FT_Set_Char_Size(face, char_width, char_height, horz_resolution, vert_resolution));
#endif

    Font_Metrics metrics;
    const FT_Fixed scale = face->size->metrics.y_scale;
    metrics.ascent_px = FT_CEIL(FT_MulFix(face->ascender, scale));
    metrics.descent_px = FT_CEIL(FT_MulFix(face->descender, scale));
    metrics.line_height_px = FT_CEIL(FT_MulFix(face->height, scale));
    metrics.y_max_px = FT_CEIL(FT_MulFix(face->bbox.yMax, scale));
    metrics.underline_offset_px = FT_CEIL(FT_MulFix(face->underline_position, scale));
    metrics.underline_thickness_px = FT_CEIL(FT_MulFix(face->underline_thickness, scale));
    metrics.max_advance_px = FT_CEIL(FT_MulFix(face->max_advance_width, scale));
    return metrics;
}

struct Font_Page
{
    unsigned glyphs_count_x = 16;
    unsigned glyphs_count_y = 8;
    // "Page" that contains 16x8 (^) glyphs, starting from `code_point_start`.
    std::uint32_t code_point_start = 0;
    unsigned glyph_max_size_px = 0;
    ImageRef atlas_rgba_;
    std::vector<GlyphInfo> glyph_list_;
    
    unsigned glyphs_count() const
    {
        return glyphs_count_x * glyphs_count_y;
    }

    bool has_code_point(std::uint32_t code_point) const
    {
        const std::uint32_t start = code_point_start;
        const std::uint32_t end = (start + glyphs_count());
        return ((code_point >= start) && (code_point < end));
    }
};

void Font::set_size(const Font_Size& new_size)
{
    if (new_size == size_)
        return;
    size_ = new_size;
    page_list_ = {};
    metrics_ = {};
    FT_Face face = static_cast<FT_Face>(ft_face_);
    KK_VERIFY(face);

    // const Font_Size size_no_DPI = Font_Size::Points(size_.pts(), Font_Size::DPI_Default);
    metrics_ = Font_QueryMetrics(face, size_);

#if (1)
    // Insert ASCII page by default.
    const std::uint32_t ascii_start_code_point = 0;
    (void)get_or_create_font_page(ascii_start_code_point);
#endif
}

Font_Metrics Merge_Metrics(const Font_Metrics& lhs, const Font_Metrics& rhs)
{
    Font_Metrics m;
    m.line_height_px = (std::max)(lhs.line_height_px, rhs.line_height_px);
    m.ascent_px = (std::max)(lhs.ascent_px, rhs.ascent_px);
    KK_VERIFY(lhs.descent_px <= 0);
    KK_VERIFY(rhs.descent_px <= 0);
    m.descent_px = (std::min)(lhs.descent_px, rhs.descent_px);
    m.y_max_px = (std::max)(lhs.y_max_px, rhs.y_max_px);
    KK_VERIFY(lhs.underline_offset_px <= 0);
    KK_VERIFY(rhs.underline_offset_px <= 0);
    m.underline_offset_px = (std::min)(lhs.underline_offset_px, rhs.underline_offset_px);
    m.underline_thickness_px = (std::max)(lhs.underline_thickness_px, rhs.underline_thickness_px);
    m.max_advance_px = (std::max)(lhs.max_advance_px, rhs.max_advance_px);
    return m;
}

Font_Size Merge_Sizes(const Font_Size& lhs, const Font_Size& rhs)
{
    Font_Size s;
    s.DPI = (std::max)(lhs.DPI, rhs.DPI);
    s.size_pt = (std::max)(lhs.size_pt, rhs.size_pt);
    s.size_px = (std::max)(lhs.size_px, rhs.size_px);
    return s;
}

Font::Font() noexcept = default;

Font::~Font() noexcept
{
    if (!ft_face_)
        return;
    KK_VERIFY(!FT_Done_Face(static_cast<FT_Face>(ft_face_)));
    ft_face_ = nullptr;
}

Font::Font(Font&& rhs) noexcept
    : ft_face_(std::exchange(rhs.ft_face_, nullptr))
    , image_factory_(std::exchange(rhs.image_factory_, {}))
    , page_list_(std::exchange(rhs.page_list_, {}))
    , size_(std::exchange(rhs.size_, {}))
    , metrics_(std::exchange(rhs.metrics_, {}))
    , has_kerning_(std::exchange(rhs.has_kerning_, {}))
{
}

Font& Font::operator=(Font&& rhs) noexcept
{
    if (this != &rhs)
    {
        this->~Font(); // I'm lazy.
        new(this) Font(std::move(rhs));
    }
    return *this;
}

// FT_PIXEL_MODE_GRAY for now.
struct Bitmap_Pixel
{
    std::uint8_t gray;
};

static ImageRef FR_BitmapToImageRGBA(const ImageFactory_RGBA& image_factory
    , const Font_Page& page
    , const std::vector<Bitmap_Pixel>& pixels)
{
    auto pack_color32 = [](std::uint8_t R, std::uint8_t G, std::uint8_t B, std::uint8_t A) -> std::uint32_t
    {
        return ((std::uint32_t(A) << 24)
              | (std::uint32_t(B) << 16)
              | (std::uint32_t(G) << 8)
              | (std::uint32_t(R) << 0));
    };

    const unsigned width_px = page.glyph_max_size_px * page.glyphs_count_x;
    const unsigned height_px = page.glyph_max_size_px * page.glyphs_count_y;

    std::vector<std::uint32_t> data_u32;
    data_u32.resize(width_px * height_px);
    const Bitmap_Pixel* src = pixels.data();
    std::uint32_t* dst = data_u32.data();
    for (int n = width_px * height_px; n > 0; n--)
    {
        const Bitmap_Pixel& p = *src++;
        *dst++ = pack_color32(0xff, 0xff, 0xff, std::uint32_t(p.gray));
    }

    // ImageRef::FromMemory(render
    //     , ImageRef::Format::RGBA
    //     , width_px
    //     , height_px
    //     , data_u32.data());
    return image_factory(width_px, height_px, data_u32.data());
}

static void FR_RenderFontPage(Font_Page& page
    , const ImageFactory_RGBA& image_factory
    , FT_Face face
    , std::uint32_t start_code_point)
{
    const FT_Fixed scale = face->size->metrics.y_scale;
    const unsigned glyph_max_width = unsigned(FT_CEIL(face->bbox.xMax - face->bbox.xMin));
    const unsigned glyph_max_height = unsigned(FT_CEIL(FT_MulFix(face->bbox.yMax - face->bbox.yMin, scale)));
    page.glyph_max_size_px = (std::max)(glyph_max_width, glyph_max_height);
    page.code_point_start = start_code_point;
    page.glyph_list_.resize(page.glyphs_count());

    const unsigned width_px = page.glyph_max_size_px * page.glyphs_count_x;
    const unsigned height_px = page.glyph_max_size_px * page.glyphs_count_y;
    std::vector<Bitmap_Pixel> pixels;
    pixels.resize(width_px * height_px);

    for (unsigned index = 0; index < page.glyphs_count(); ++index)
    {
        const std::uint32_t code_point = (start_code_point + index);
        FT_UInt glyph_index = FT_Get_Char_Index(face, code_point);
        KK_VERIFY(!FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT));
        KK_VERIFY(!FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL));
        KK_VERIFY(face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
        const FT_Bitmap& bitmap = face->glyph->bitmap;
        KK_VERIFY(bitmap.width <= page.glyph_max_size_px);
        KK_VERIFY(bitmap.rows <= page.glyph_max_size_px);

        const unsigned glyph_offset_x = (index % page.glyphs_count_x) * page.glyph_max_size_px;
        const unsigned glyph_offset_y = (index / page.glyphs_count_x) * page.glyph_max_size_px;
        Bitmap_Pixel* dst_ptr = pixels.data() + (glyph_offset_y * width_px) + glyph_offset_x;
        unsigned char* U8_src_ptr = bitmap.buffer;
        for (unsigned i = 0; i < bitmap.rows; ++i)
        {
            for (unsigned j = 0; j < bitmap.width; ++j)
            {
                Bitmap_Pixel& p = dst_ptr[j];
                p.gray = std::uint8_t(U8_src_ptr[j]);
            }
            dst_ptr += width_px;
            U8_src_ptr += bitmap.pitch; // pitch is in bytes
        }

        GlyphInfo& glyph = page.glyph_list_[index];
        glyph.glyph_index = glyph_index;
        glyph.size.x = bitmap.width;
        glyph.size.y = bitmap.rows;
        glyph.bitmap_delta.x = face->glyph->bitmap_left;
        glyph.bitmap_delta.y = face->glyph->bitmap_top;
        // `advance`: 26.6 fractional pixel format,
        // which means 1 unit is equal to 1/64th of a pixel.
        glyph.advance.x = FT_CEIL(face->glyph->advance.x);
        glyph.advance.y = FT_CEIL(face->glyph->advance.y);
    }

    page.atlas_rgba_ = FR_BitmapToImageRGBA(image_factory, page, pixels);
}

Font_Page& Font::get_or_create_font_page(std::uint32_t code_point)
{
    // #TODO: Needs to be O(1).
    for (Font_Page& existing_page : page_list_)
    {
        if (existing_page.has_code_point(code_point))
            return existing_page;
    }

    auto get_page_start_code_point = [](Font_Page& page, std::uint32_t code_point)
    {
        const std::int64_t half_page_glyphs = (std::int64_t(page.glyphs_count()) / 2);
        const std::int64_t start = std::int64_t(code_point) - half_page_glyphs;
        return std::uint32_t((std::max)(std::int64_t(0), start));
    };

    Font_Page& page = page_list_.emplace_back();
    const std::uint32_t start = get_page_start_code_point(page, code_point);
    FR_RenderFontPage(page
        , image_factory_
        , static_cast<FT_Face>(ft_face_)
        , start);
    return page;
}

static kk::Vec2f Glyph_UV(const Font_Page& page, std::uint32_t code_point, kk::Vec2u offset)
{
    KK_VERIFY(code_point >= page.code_point_start);
    KK_VERIFY(code_point < (page.code_point_start + page.glyphs_count()));
    const unsigned glyph_index = (code_point - page.code_point_start);

    const unsigned x = (glyph_index % page.glyphs_count_x);
    const unsigned y = (glyph_index / page.glyphs_count_x);
    kk::Vec2f uv_start;
    uv_start.x = x / float(page.glyphs_count_x);
    uv_start.y = y / float(page.glyphs_count_y);

    kk::Vec2f uv = uv_start;
    uv.x += (offset.x / float(page.glyph_max_size_px)) / page.glyphs_count_x;
    uv.y += (offset.y / float(page.glyph_max_size_px)) / page.glyphs_count_y;
    return uv;
}

kk::Point Font::kerning_delta(GlyphIndex left_glyph, GlyphIndex right_glyph) const
{
    if (!has_kerning_)
        return {};
    if ((left_glyph == 0) || (right_glyph == 0))
        return {};
    FT_Face face = static_cast<FT_Face>(ft_face_);
    FT_Vector delta{};
    KK_VERIFY(!FT_Get_Kerning(face
        , FT_UInt(left_glyph)
        , FT_UInt(right_glyph)
        , FT_KERNING_DEFAULT
        , &delta));
    const int x_delta_px = FT_CEIL(delta.x);
    const int y_delta_px = FT_CEIL(delta.y);
    return kk::Point{x_delta_px, y_delta_px};
}

GlyphRender Font::glyph_render(std::uint32_t code_point)
{
    Font_Page& page = get_or_create_font_page(code_point);
    KK_VERIFY(code_point >= page.code_point_start);
    KK_VERIFY(code_point < (page.code_point_start + page.glyphs_count()));
    const int index = (code_point - page.code_point_start);
    const GlyphInfo& char_ = page.glyph_list_[index];

    const kk::Vec2f uv_start = Glyph_UV(page, code_point, kk::Vec2u{0, 0});
    const kk::Vec2f uv_end = Glyph_UV(page, code_point, char_.size);

    GlyphRender state;
    state.glyph_info = page.glyph_list_[index];
    state.uv = kk::Rect2f::From(uv_start, uv_end);
    state.texture = page.atlas_rgba_;
    return state;
}

const GlyphInfo& Font::glyph_info(std::uint32_t code_point)
{
    Font_Page& page = get_or_create_font_page(code_point);
    KK_VERIFY(code_point >= page.code_point_start);
    KK_VERIFY(code_point < (page.code_point_start + page.glyphs_count()));
    const int index = (code_point - page.code_point_start);
    const GlyphInfo& char_ = page.glyph_list_[index];
    return char_;
}

Font Font_FromFile(Font_FreeTypeLibrary& font_init
    , KidsRender& render
    , const char* ttf_file_path
    , const Font_Size& size)
{
    auto image_factory = [&render](int width_px, int height_px, const void* data)
    {
        return ImageRef::FromMemory(render
            , ImageRef::Format::RGBA
            , width_px
            , height_px
            , data);
    };
    return Font::FromFile(font_init, image_factory, ttf_file_path, size);
}

} // namespace kr

