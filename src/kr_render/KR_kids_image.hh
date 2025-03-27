#pragma once
#include "KR_kids_config.hh"

#include <memory>

namespace kr
{

struct KidsRender;

class ImageRef
{
public:
    enum class Format
    {
#if (!KK_RENDER_VULKAN())
        RGB = 1,
#endif
        RGBA = 2
    };

    static ImageRef FromMemory(KidsRender& render
        , Format format
        , int width
        , int height
        , const void* data);

public:
    int width() const;
    int height() const;

#if (KK_RENDER_OPENGL())
    unsigned handle() const;
#endif
#if (KK_RENDER_VULKAN())
    VkImageView image_view() const;
#endif

    bool is_valid() const { return !!ref_; }

    friend bool operator==(const ImageRef& lhs, const ImageRef& rhs) noexcept;
    friend bool operator!=(const ImageRef& lhs, const ImageRef& rhs) noexcept;

private:
    struct ImageState;
    std::shared_ptr<ImageState> ref_;
};

inline bool operator==(const ImageRef& lhs, const ImageRef& rhs) noexcept
{
    return (lhs.ref_ == rhs.ref_);
}
inline bool operator!=(const ImageRef& lhs, const ImageRef& rhs) noexcept
{
    return (lhs.ref_ != rhs.ref_);
}

} // namespace kr
