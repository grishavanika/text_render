#pragma once

#if defined(KK_BUILD_RENDER_OPENGL) && (KK_BUILD_RENDER_OPENGL == 1)
#  define KK_RENDER_OPENGL() 1
#  include <glad/glad.h>
#else
#  define KK_RENDER_OPENGL() 0
#endif
#if defined(KK_BUILD_RENDER_VULKAN) && (KK_BUILD_RENDER_VULKAN == 1)
#  define KK_RENDER_VULKAN() 1
#  include <vulkan/vulkan.h>
#else
#  define KK_RENDER_VULKAN() 0
#endif

static_assert(0
    + int(KK_RENDER_OPENGL())
    + int(KK_RENDER_VULKAN())
    == 1
    , "KK_RENDER_*: only one backend should be selected.");

#include "KS_basic_math.hh"
#include "KS_asserts.hh"

namespace kr
{

struct ClipRect : ::kk::Rect2f
{
    ClipRect() noexcept
    {
        width = -1.f;
        height = -1.f;
    }
    ClipRect(const ::kk::Rect2f& r) noexcept
    {
        static_cast<::kk::Rect2f&>(*this) = r;
    }
};

} // namespace kr

#if (KK_RENDER_VULKAN())
namespace kk::detail
{
inline bool CastToOk(VkResult vk_r)
{
    return (vk_r == VK_SUCCESS);
}
} // namespace kk::detail
#endif
