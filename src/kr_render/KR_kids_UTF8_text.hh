#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include "KR_kids_config.hh"

namespace kr
{

struct Text_UTF8
{
    const char* text_start_ = nullptr;
    const char* text_end_ = nullptr;
    explicit Text_UTF8() = default;
    explicit Text_UTF8(const char* text_start, const char* text_end)
        : text_start_{text_start}
        , text_end_{text_end} {}
    explicit Text_UTF8(const char* text_start)
        : Text_UTF8(text_start, text_start + strlen(text_start)) {}

    int bytes_count() const { return int(text_end_ - text_start_); }
};

// ImTextCharFromUtf8().
// From https://github.com/ocornut/imgui/blob/9779cc2fe273c4ea96b9763fcd54f14b8cbae561/imgui.cpp#L1842
// Convert UTF-8 to 32-bit character, process single character input.
// A nearly-branchless UTF-8 decoder, based on work of Christopher Wellons (https://github.com/skeeto/branchless-utf8).
// We handle UTF-8 decoding error by skipping forward.
inline int UTF8_Decode(std::uint32_t* out_code_point, const char* in_text, const char* in_text_end)
{
    static const char lengths[32] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0 };
    static const int masks[]  = { 0x00, 0x7f, 0x1f, 0x0f, 0x07 };
    static const std::uint32_t mins[] = { 0x400000, 0, 0x80, 0x800, 0x10000 };
    static const int shiftc[] = { 0, 18, 12, 6, 0 };
    static const int shifte[] = { 0, 6, 4, 2, 0 };
    int len = lengths[*(const unsigned char*)in_text >> 3];
    int wanted = len + !len;

    if (in_text_end == nullptr)
        in_text_end = in_text + wanted; // Max length, nulls will be taken into account.

    // Copy at most 'len' bytes, stop copying at 0 or past in_text_end. Branch predictor does a good job here,
    // so it is fast even with excessive branching.
    std::uint8_t s[4];
    s[0] = in_text + 0 < in_text_end ? std::uint8_t(in_text[0]) : 0;
    s[1] = in_text + 1 < in_text_end ? std::uint8_t(in_text[1]) : 0;
    s[2] = in_text + 2 < in_text_end ? std::uint8_t(in_text[2]) : 0;
    s[3] = in_text + 3 < in_text_end ? std::uint8_t(in_text[3]) : 0;

    // Assume a four-byte character and load four bytes. Unused bits are shifted out.
    *out_code_point  = (std::uint32_t)(s[0] & masks[len]) << 18;
    *out_code_point |= (std::uint32_t)(s[1] & 0x3f) << 12;
    *out_code_point |= (std::uint32_t)(s[2] & 0x3f) <<  6;
    *out_code_point |= (std::uint32_t)(s[3] & 0x3f) <<  0;
    *out_code_point >>= shiftc[len];

    // Accumulate the various error conditions.
    int e = 0;
    e  = (*out_code_point < mins[len]) << 6; // non-canonical encoding
    e |= ((*out_code_point >> 11) == 0x1b) << 7;  // surrogate half?
    e |= (*out_code_point > 0x10FFFF) << 8;  // out of range?
    e |= (s[1] & 0xc0) >> 2;
    e |= (s[2] & 0xc0) >> 4;
    e |= (s[3]       ) >> 6;
    e ^= 0x2a; // top two bits of each tail byte correct?
    e >>= shifte[len];

    if (e)
    {
        // No bytes are consumed when *in_text == 0 || in_text == in_text_end.
        // One byte is consumed in case of invalid first byte of in_text.
        // All available bytes (at most `len` bytes) are consumed on incomplete/invalid second to last bytes.
        // Invalid or incomplete input may consume less bytes than wanted, therefore every byte has to be inspected in s.
        wanted = (std::min)(wanted, !!s[0] + !!s[1] + !!s[2] + !!s[3]);
        *out_code_point = 0; // Error code point.
    }

    return wanted;
}

inline bool UTF8_IsTrail(char ch)
{
    return ((std::uint8_t(0xff & ch) >> 6) == 0x2);
}

inline const char* UTF8_Prev(const char* str, const char* str_begin)
{
    if (str <= str_begin)
        return nullptr;
    while (UTF8_IsTrail(*(--str)))
    {
        if (str <= str_begin)
            return nullptr;
    }
    return str;
}

struct LineCodepointMeta
{
    std::uint32_t codepoint = 0; // '\n' for new line
    kr::Text_UTF8 current_line{};
    kr::Text_UTF8 codepoint_part{};
};

template<typename F> // void F(const LineCodepointMeta&)
void UTF8_IterateLines(const Text_UTF8& text, F&& f, bool use_crlf)
{
    const char* text_start = text.text_start_;
    const char* const text_end = text.text_end_;
    const char* line_start = text_start;
    while (text_start < text_end)
    {
        std::uint32_t codepoint = 0;
        const char* const decode_start = text_start;
        int step = UTF8_Decode(&codepoint, text_start, text_end);
        if (codepoint == 0)
        {
            KK_VERIFY(false);
            break;
        }

        bool new_line = false;
        if (use_crlf && (codepoint == '\r'))
        {
            std::uint32_t next_code_point = 0;
            const int next_step = UTF8_Decode(&next_code_point, text_start + step, text_end);
            if (next_code_point == '\n')
            {
                step += next_step;
                codepoint = next_code_point;
                new_line = true;
            }
        }
        else
            new_line = (codepoint == '\n');
        const char* const decode_end = (decode_start + step);

        if (new_line)
        {
            LineCodepointMeta meta;
            meta.codepoint = codepoint;
            meta.current_line = Text_UTF8{line_start, text_start};
            meta.codepoint_part = Text_UTF8{decode_start, decode_end};
            f(meta);
            text_start += step;
            line_start = text_start;
            if (text_start == text_end)
            { // Last, empty line.
                meta.current_line = Text_UTF8{text_end, text_end};
                meta.codepoint_part = Text_UTF8{text_end, text_end};
                f(meta);
            }
        }
        else
        {
            text_start += step;
            LineCodepointMeta meta;
            meta.codepoint = codepoint;
            meta.current_line = Text_UTF8{line_start, text_start};
            meta.codepoint_part = Text_UTF8{decode_start, decode_end};
            f(meta);
        }
    }
}

} // namespace kr
