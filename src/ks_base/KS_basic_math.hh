#pragma once
#include "KS_asserts.hh"

#include <algorithm> // std::min/max/clamp
#include <limits.h> // INT_MIN/INT_MAX

#include <cstdint>
#include <cmath>

namespace kk
{

struct Vec2i
{
    int x = 0;
    int y = 0;

    Vec2i& operator+=(const Vec2i& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Vec2i& operator-=(const Vec2i& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
};

struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};

struct Vec2f
{
    float x = 0;
    float y = 0;

    Vec2f& operator+=(const Vec2f& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    Vec2f& operator-=(const Vec2f& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
};

struct Vec4f
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

struct Vec2u
{
    unsigned x = 0;
    unsigned y = 0;
};

inline bool operator==(const Color& lhs, const Color& rhs)
{
    return (lhs.r == rhs.r)
        && (lhs.g == rhs.g)
        && (lhs.b == rhs.b)
        && (lhs.a == rhs.a);
}

inline bool operator!=(const Color& lhs, const Color& rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(const Vec2f& lhs, const Vec2f& rhs)
{
    return (lhs.x == rhs.x)
        && (lhs.y == rhs.y);
}

inline bool operator!=(const Vec2f& lhs, const Vec2f& rhs)
{
    return !(lhs == rhs);
}

inline bool operator==(const Vec2i& lhs, const Vec2i& rhs)
{
    return (lhs.x == rhs.x)
        && (lhs.y == rhs.y);
}

inline bool operator!=(const Vec2i& lhs, const Vec2i& rhs)
{
    return !(lhs == rhs);
}

inline Vec2i operator+(const Vec2i& lhs, const Vec2i& rhs)
{
    return Vec2i{.x = (lhs.x + rhs.x), .y = (lhs.y + rhs.y)};
}

inline Vec2i operator-(const Vec2i& lhs, const Vec2i& rhs)
{
    return Vec2i{.x = (lhs.x - rhs.x), .y = (lhs.y - rhs.y)};
}

inline Vec2f operator+(const Vec2f& v1, const Vec2f& v2)
{
    return Vec2f{v1.x + v2.x, v1.y + v2.y};
}

inline Vec2f operator-(const Vec2f& v1, const Vec2f& v2)
{
    return Vec2f{v1.x - v2.x, v1.y - v2.y};
}

inline Vec2f operator*(const Vec2f& v, float k)
{
    return Vec2f{v.x * k, v.y * k};
}

inline Vec2f operator/(const Vec2f v, float t)
{
    return v * (1 / t);
}

inline Vec2f Vec2f_From2i(const Vec2i& v)
{
    return Vec2f{float(v.x), float(v.y)};
}

inline float Vec2f_Dot(const Vec2f& v1, const Vec2f& v2)
{
    return v1.x * v2.x
        +  v1.y * v2.y;
}

inline float Vec2f_Length2(const Vec2f& v)
{
    return v.x * v.x + v.y * v.y;
}

inline float Vec2f_Length(const Vec2f& v)
{
    return sqrtf(Vec2f_Length2(v));
}

inline Vec2f Vec2f_Normalize(const Vec2f& v)
{
    return v / Vec2f_Length(v);
}

struct Size
{
    int width = 0;
    int height = 0;

    static Size From(const Vec2i& min_, const Vec2i& max_)
    {
        return Size{.width = (max_.x - min_.x), .height = (max_.y - min_.y)};
    }

    friend bool operator==(const Size& lhs, const Size& rhs)
    {
        return (lhs.width == rhs.width)
            && (lhs.height == rhs.height);
    }

    friend bool operator!=(const Size& lhs, const Size& rhs)
    {
        return !(lhs == rhs);
    }
};

struct Size2f
{
    float width = 0.f;
    float height = 0.f;

    static Size2f From(const Vec2f& min_, const Vec2f& max_)
    {
        return Size2f{.width = (max_.x - min_.x), .height = (max_.y - min_.y)};
    }

    friend bool operator==(const Size2f& lhs, const Size2f& rhs)
    {
        return (lhs.width == rhs.width)
            && (lhs.height == rhs.height);
    }

    friend bool operator!=(const Size2f& lhs, const Size2f& rhs)
    {
        return !(lhs == rhs);
    }
};

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    static Rect From(const Vec2i& position, const Size& size)
    {
        return Rect{.x = position.x, .y = position.y, .width = size.width, .height = size.height};
    }

    static Rect From(const Vec2i& min_, const Vec2i& max_)
    {
        return From(min_, Size::From(min_, max_));
    }

    friend bool operator==(const Rect& lhs, const Rect& rhs)
    {
        return (lhs.x == rhs.x)
            && (lhs.y == rhs.y)
            && (lhs.width == rhs.width)
            && (lhs.height == rhs.height);
    }

    friend bool operator!=(const Rect& lhs, const Rect& rhs)
    {
        return !(lhs == rhs);
    }

    Size size() const { return Size{.width = width, .height = height}; }
    Vec2i position() const { return Vec2i{.x = x, .y = y}; }
    // Hello, Windows.h.
    Vec2i (min)() const { return Vec2i{.x = x, .y = y}; }
    Vec2i (max)() const { return Vec2i{.x = x + width, .y = y + height}; }
};

struct Rect2f
{
    float x = 0;
    float y = 0;
    float width = 0;
    float height = 0;

    static Rect2f From(const Vec2f& position, const Size2f& size)
    {
        return Rect2f{.x = position.x, .y = position.y, .width = size.width, .height = size.height};
    }

    static Rect2f From(const Vec2f& min_, const Vec2f& max_)
    {
        return From(min_, Size2f::From(min_, max_));
    }

    friend bool operator==(const Rect2f& lhs, const Rect2f& rhs)
    {
        return (lhs.x == rhs.x)
            && (lhs.y == rhs.y)
            && (lhs.width == rhs.width)
            && (lhs.height == rhs.height);
    }

    friend bool operator!=(const Rect2f& lhs, const Rect2f& rhs)
    {
        return !(lhs == rhs);
    }

    Size2f size() const { return Size2f{.width = width, .height = height}; }
    Vec2f position() const { return Vec2f{.x = x, .y = y}; }
    // Hello, Windows.h.
    Vec2f (min)() const { return Vec2f{.x = x, .y = y}; }
    Vec2f (max)() const { return Vec2f{.x = x + width, .y = y + height}; }
};

using Point = Vec2i;
using Point2f = Vec2f;

// #TODO: Use static inline constexpr.
inline Point2f Point2f_Invalid() { return Point2f{FLT_MIN, FLT_MIN}; }
inline Color Color_White() { return Color{0xff, 0xff, 0xff, 0xff}; }
inline Color Color_Black() { return Color{0x00, 0x00, 0x00, 0xff}; }
inline Color Color_Red() { return Color{0xff, 0x00, 0x00, 0xff}; }
inline Color Color_Green() { return Color{0x00, 0xff, 0x00, 0xff}; }
inline Color Color_Blue() { return Color{0x00, 0x00, 0xff, 0xff}; }
inline Color Color_Yellow() { return Color{0xff, 0xff, 0x00, 0xff}; }

} // namespace kk
