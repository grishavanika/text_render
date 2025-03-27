#include "os_window.hh"
#include "os_window_UTILS.hh"
#include "os_render_backend.hh"
#include "KR_kids_font_fallback.hh"
#include "KR_kids_UTF8_text.hh"
#include "KR_text_shaper.hh"

#define KK_TEXT_DEFAULT() 0
#define KK_TEXT_RENDER_BASELINE() KK_TEXT_DEFAULT()
#define KK_TEXT_COLOR_LINES() KK_TEXT_DEFAULT()
#define KK_TEXT_MIN_RECT() KK_TEXT_DEFAULT()
#define KK_TEXT_BY_HEIGHT_RECT() KK_TEXT_DEFAULT()
#define KK_TEXT_INITIAL_POSITION() KK_TEXT_DEFAULT()

enum class Text_Offset
{
    None,
    ByMinRect,
    ByLineHeight,
};

void Render_Text(kr::KidsRender& render
    , kr::Font_Family& font_family
    , const kk::Color& color = kk::Color_Black()
    , Text_Offset offset = Text_Offset::None
    , const kk::Point2f text_p = {50.f, 150.f}
    )
{
    kr::Text_Shaper shaper;
    shaper.render_ = &render;
    shaper.disable_kerning_ = true;
    {
        kr::Text_Markup markup;
        markup.color_ = {0xff, 0x00, 0x00, 0xff};
        markup.font_fallback_ = &font_family.select_font({.bold = false, .italic = false});
        markup.strikethrough_color_ = {0xff, 0xff, 0x00, 0xff};
        kr::Text_Markup markup_background = markup;
        markup_background.background_color_ = {0x00, 0x00, 0xff, 0xff};
        shaper.text_add(kr::Text_UTF8{"SOME T"}, markup);
        shaper.text_add(kr::Text_UTF8{"e"}, markup_background);
        shaper.text_add(kr::Text_UTF8{"xt To"}, markup);
    }
    {
        kr::Text_Markup markup;
        markup.color_ = color;
        markup.font_fallback_ = &font_family.select_font({.bold = true, .italic = false});
        shaper.text_add(kr::Text_UTF8{" RENDER "}, markup);
    }
    {
        kr::Text_Markup markup;
        markup.color_ = color;
        markup.font_fallback_ = &font_family.select_font({.bold = false, .italic = true});
        markup.underline_color_ = {0x00, 0xff, 0x00, 0xff};
        shaper.text_add(kr::Text_UTF8{"ppp"}, markup);
    }
    {
        kr::Text_Markup markup;
        markup.color_ = {0xff, 0x00, 0x00, 0xff};
        markup.font_fallback_ = &font_family.select_font({.bold = false, .italic = false});
        kr::Text_Markup markup_color = markup;;
        markup_color.background_color_ = {0x00, 0x00, 0xff, 0xff};
        shaper.text_add(kr::Text_UTF8{"\nNe"}, markup);
        shaper.text_add(kr::Text_UTF8{"w lin"}, markup_color);
        shaper.text_add(kr::Text_UTF8{"e."}, markup);
    }
    shaper.finish();

#if (KK_TEXT_INITIAL_POSITION())
    {
        const kk::Point2f start = (text_p + Vec2f_From2i(shaper.start_point()));
        kk::Point2f p_min = start;
        kk::Point2f p_max = start;
        p_min.x -= 20;
        p_max.x += shaper.metrics_min().rect.size().width;
        p_max.x += 20;
        render.line(p_min, p_max, {0x00, 0xff, 0xff, 0xff});
        render.circle_fill(start, 5, {0xff, 0x00, 0x00, 0xff});
    }
#endif

    const kk::Point2f baseline_offset = [&]()
    {
        switch (offset)
        {
        case Text_Offset::None:
            return kk::Point2f{0, 0};
        case Text_Offset::ByMinRect:
            return Vec2f_From2i(shaper.metrics_min().baseline_offset());
        case Text_Offset::ByLineHeight:
            return Vec2f_From2i(shaper.metrics().baseline_offset());
        }
        KK_UNREACHABLE();
    }();

    auto do_translate = [&](kk::Rect r)
    {
        kk::Rect2f o{};
        o.x = r.x;
        o.y = r.y;
        o.height = r.height;
        o.width = r.width;
        o.x += text_p.x;
        o.y += text_p.y;
        o.x += baseline_offset.x;
        o.y += baseline_offset.y;
        return o;
    };
    (void)do_translate;

    shaper.draw(text_p + baseline_offset);

#if (KK_TEXT_MIN_RECT())
    const kk::Rect2f r_min = do_translate(shaper.metrics_min().rect);
    render.rect(r_min.min(), r_min.max(), {0xff, 0x00, 0x00, 0xff});
#endif
#if (KK_TEXT_BY_HEIGHT_RECT())
    const kk::Rect2f r_by_line_height = do_translate(shaper.metrics().rect);
    render.rect(r_by_line_height.min(), r_by_line_height.max(), {0xff, 0x00, 0xff, 0xff});
#endif

    const std::vector<kr::Text_ShaperLine>& line_list = shaper.line_list_;
    (void)line_list;
#if (KK_TEXT_RENDER_BASELINE())
    kk::Point2f b_min = (text_p + baseline_offset + Vec2f_From2i(shaper.start_point()));
    kk::Point2f b_max = b_min;
    b_min.x += shaper.metrics_min().rect.size().width;
    for (const kr::Text_ShaperLine& line : line_list)
    {
        render.line(b_min, b_max, {0x00, 0xff, 0x00, 0xff});
        b_min.y += line.metrics_.line_height_px;
        b_max.y += line.metrics_.line_height_px;
    }
#endif
#if (KK_TEXT_COLOR_LINES())
    KK_VERIFY(line_list.size() > 0);
    kk::Rect2f r = do_translate(shaper.metrics().rect);
    r.height = line_list.front().metrics_.line_height_px;
    const kk::Color c1{0xff, 0xff, 0x00, 0x44};
    const kk::Color c2{0xff, 0x00, 0xff, 0x44};
    kk::Color c = c1;
    for (const kr::Text_ShaperLine& line : line_list)
    {
        render.rect_fill(r.min(), r.max(), c);
        c = (c == c1) ? c2 : c1;
        r.y = r.max().y;
        r.height = line.metrics_.line_height_px;
    }
#endif
}

int main()
{
    const kr::Font_Size kFontSize = kr::Font_Size::Points(48);
    //const char kFontFile1[] = R"(C:\Windows\Fonts\msyh.ttc)";
    //const char kFontFile2[] = R"(C:\Windows\Fonts\seguiemj.ttf)";
    //const char kFontFile3[] = R"(C:\Windows\Fonts\times.ttf)";
    
    OsRender os_render;
    KK_VERIFY(os_render.state);

    OsWindow window;
    kr::KidsRender render1;
    OsRender_WindowCreate(os_render.state, window);
    OsRender_Build(os_render.state, window, render1);
    
    kr::Font_FreeTypeLibrary font_init;
    auto make_font = [&](const char* file_path)
    {
        kr::Font_Fallback font;
        font.set_main_font(kr::Font_FromFile(font_init, render1, file_path, kFontSize));
        return font;
    };

#if (1)
    kr::Font_Fallback font = make_font(R"(C:\Windows\Fonts\consola.ttf)");
    kr::Font_Fallback font_bold = make_font(R"(C:\Windows\Fonts\consolab.ttf)");
    kr::Font_Fallback font_italic = make_font(R"(C:\Windows\Fonts\consolai.ttf)");
    kr::Font_Fallback font_bold_italic = make_font(R"(C:\Windows\Fonts\consolaz.ttf)");
#elif (0)
    kr::Font_Fallback font = make_font(R"(K:\Roboto_Mono\static\RobotoMono-Regular.ttf)");
    kr::Font_Fallback font_bold = make_font(R"(K:\Roboto_Mono\static\RobotoMono-Bold.ttf)");
    kr::Font_Fallback font_italic = make_font(R"(K:\Roboto_Mono\static\RobotoMono-Italic.ttf)");
    kr::Font_Fallback font_bold_italic = make_font(R"(K:\Roboto_Mono\static\RobotoMono-BoldItalic.ttf)");
#else
    kr::Font_Fallback font = make_font(R"(K:\Source_Code_Pro\static\SourceCodePro-Regular.ttf)");
    kr::Font_Fallback font_bold = make_font(R"(K:\Source_Code_Pro\static\SourceCodePro-Bold.ttf)");
    kr::Font_Fallback font_italic = make_font(R"(K:\Source_Code_Pro\static\SourceCodePro-Italic.ttf)");
    kr::Font_Fallback font_bold_italic = make_font(R"(K:\Source_Code_Pro\static\SourceCodePro-BoldItalic.ttf)");
#endif

    kr::Font_Family font_family;
    font_family.font = &font;
    font_family.font_bold = &font_bold;
    font_family.font_italic = &font_italic;
    font_family.font_bold_italic = &font_bold_italic;

    OsWindow window2(&window);
    kr::KidsRender render2;
    OsRender_WindowCreate(os_render.state, window2);
    OsRender_Build(os_render.state, window2, render2);

    auto should_close = [&]()
    {
        return !Wnd_IsAlive(window) || !Wnd_IsAlive(window2);
    };

    while (true)
    {
        Wnd_AllEventsPoll();
        if (should_close())
            break;

        OsRender_NewFrame(os_render.state
            , window
            , {0xff, 0xff, 0xff, 0x00}
            , [&](OsWindow& /*window*/, kr::FrameInfo& frame_info)
        {
            kr::KidsRender& render = render1;
            render.rect_fill({50, 450}, {500, 700});
            render.rect({50, 450}, {500, 700}, kk::Color_Black());
            Render_Text(render
                , font_family
                , kk::Color_Black()
                , Text_Offset::ByMinRect
                );
            render.draw(frame_info);
            render.clear();
        });

        OsRender_NewFrame(os_render.state
            , window2
            , {0x00, 0x00, 0x00, 0x00}
            , [&](OsWindow& /*window*/, kr::FrameInfo& frame_info)
        {
            kr::KidsRender& render = render2;
            render.rect_fill({50, 450}, {500, 700});
            render.rect({50, 450}, {500, 700}, kk::Color_Black());
            Render_Text(render
                , font_family
                , kk::Color_White()
                , Text_Offset::ByLineHeight
                );
            render.draw(frame_info);
            render.clear();
        });

        OsRender_Present(os_render.state, window);
        OsRender_Present(os_render.state, window2);
    }

    OsRender_Finish(os_render.state);
    OsRender_WindowDestroy(os_render.state, window);
    OsRender_WindowDestroy(os_render.state, window2);
}
