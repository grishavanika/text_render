#include "KR_kids_image.hh"
#include "KR_kids_render.hh"

#include <utility>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace kr
{

struct ImageRef::ImageState
{
    int width_ = 0;
    int height_ = 0;

#if (KK_RENDER_OPENGL())
    // Owns.
    unsigned texture_name_ = 0;
#endif
#if (KK_RENDER_VULKAN())
    VkDevice device_{};
    // Owns.
    VkDeviceMemory memory_{};
    VkImage image_{};
    VkImageView image_view_{};
#endif

    static std::shared_ptr<ImageState> New()
    {
        return std::make_shared<ImageState>();
    }

    ImageState() = default;
    ~ImageState() noexcept;
    ImageState(const ImageState&) = delete;
    ImageState& operator=(const ImageState&) = delete;
    ImageState(ImageState&&) = delete;
    ImageState& operator=(ImageState&&) = delete;
};

int ImageRef::width() const
{
    KK_VERIFY(ref_);
    return ref_->width_;
}

int ImageRef::height() const
{
    KK_VERIFY(ref_);
    return ref_->height_;
}

#if (KK_RENDER_OPENGL())
/*static*/ ImageRef ImageRef::FromMemory(KidsRender&
    , Format format
    , int width
    , int height
    , const void* data)
{
    GLenum gl_format = GL_RGBA;
    switch (format)
    {
    case Format::RGB: gl_format = GL_RGB; break;
    case Format::RGBA: gl_format = GL_RGBA; break;
    }

    unsigned texture_name = 0;
    ::glGenTextures(1, &texture_name);
    ::glBindTexture(GL_TEXTURE_2D, texture_name);
    ::glTexImage2D(GL_TEXTURE_2D, 0, gl_format, width, height, 0, gl_format, GL_UNSIGNED_BYTE, data);
    ::glGenerateMipmap(GL_TEXTURE_2D);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ImageRef image_ref;
    image_ref.ref_ = ImageState::New();
    image_ref.ref_->width_ = width;
    image_ref.ref_->height_ = height;
    image_ref.ref_->texture_name_ = texture_name;
    return image_ref;
}

ImageRef::ImageState::~ImageState() noexcept
{
    ::glDeleteTextures(1, &texture_name_);
}

unsigned ImageRef::handle() const
{
    KK_VERIFY(ref_);
    return ref_->texture_name_;
}
#endif

#if (KK_RENDER_VULKAN())
VkImageView ImageRef::image_view() const
{
    KK_VERIFY(ref_);
    return ref_->image_view_;
}

// Mostly from ImGUI. See:
// https://github.com/ocornut/imgui/blob/26f817807cb9edfa2057a2b07a700f8e53b923fb/backends/imgui_impl_vulkan.cpp
// 

static uint32_t Vulkan_MemoryType(
    VkPhysicalDevice physical_device
    , VkMemoryPropertyFlags properties
    , uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; ++i)
    {
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    }
    KK_VERIFY(false);
    return 0xffffffff;
}

struct VulkanImage
{
    VkDeviceMemory memory_{};
    VkImage image_{};
    VkImageView image_view_{};
};

static VulkanImage Vulkan_CreateImage_RecordCmds(
      VkCommandBuffer command_buffer
    , VkBuffer* upload_buffer
    , VkDeviceMemory* upload_buffer_memory
    , VkDevice device
    , VkPhysicalDevice physical_device
    , ImageRef::Format format
    , int width
    , int height
    , const void* ptr)
{
    VulkanImage vk_image;
    VkResult err{};

    const unsigned char* pixels = static_cast<const unsigned char*>(ptr);
    size_t upload_size = width * height;
    VkFormat vk_format = VK_FORMAT_R8G8B8A8_UNORM;
    switch (format)
    {
    case ImageRef::Format::RGBA:
        upload_size *= 4;
        vk_format = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    }

    // Create the Image:
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = vk_format;
        info.extent.width = width;
        info.extent.height = height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(device, &info, nullptr, &vk_image.image_);
        KK_VERIFY(err == VK_SUCCESS);
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device, vk_image.image_, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = Vulkan_MemoryType(physical_device
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device, &alloc_info, nullptr, &vk_image.memory_);
        KK_VERIFY(err == VK_SUCCESS);
        err = vkBindImageMemory(device, vk_image.image_, vk_image.memory_, 0);
        KK_VERIFY(err == VK_SUCCESS);
    }

    // Create the Image View:
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = vk_image.image_;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = vk_format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.layerCount = 1;
        err = vkCreateImageView(device, &info, nullptr, &vk_image.image_view_);
        KK_VERIFY(err == VK_SUCCESS);
    }

    // Create the Upload Buffer:
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = upload_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(device, &buffer_info, nullptr, upload_buffer);
        KK_VERIFY(err == VK_SUCCESS);
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, *upload_buffer, &req);
        // bd->BufferMemoryAlignment = (bd->BufferMemoryAlignment > req.alignment) ? bd->BufferMemoryAlignment : req.alignment;
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = Vulkan_MemoryType(physical_device, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device, &alloc_info, nullptr, upload_buffer_memory);
        KK_VERIFY(err == VK_SUCCESS);
        err = vkBindBufferMemory(device, *upload_buffer, *upload_buffer_memory, 0);
        KK_VERIFY(err == VK_SUCCESS);
    }

    // Upload to Buffer:
    {
        void* map = nullptr;
        err = vkMapMemory(device, *upload_buffer_memory, 0, VK_WHOLE_SIZE, 0, &map);
        KK_VERIFY(err == VK_SUCCESS);
        memcpy(map, pixels, upload_size);
        VkMappedMemoryRange range[1] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = *upload_buffer_memory;
        range[0].size = VK_WHOLE_SIZE;
        err = vkFlushMappedMemoryRanges(device, 1, range);
        KK_VERIFY(err == VK_SUCCESS);
        vkUnmapMemory(device, *upload_buffer_memory);
    }

    // Copy to Image:
    {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image = vk_image.image_;
        copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount = 1;
        copy_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(command_buffer
            , *upload_buffer
            , vk_image.image_
            , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            , 1
            , &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image = vk_image.image_;
        use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount = 1;
        use_barrier[0].subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, use_barrier);
    }
    return vk_image;
}

static VulkanImage Vulkan_CreateImageTexture(
      const ImageRef& ref // Only to extend lifetime.
    , KidsRender& render
    , ImageRef::Format format
    , int width
    , int height
    , const void* ptr)
{
    VkDevice device = render.render_data_.device;
    VkDeviceMemory upload_buffer_memory{};
    VkBuffer upload_buffer{};
    VkCommandBuffer command_buffer{};

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext = nullptr;
    alloc_info.commandPool = render.render_data_.work_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    KK_VERIFY(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer) == VK_SUCCESS);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    KK_VERIFY(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);

    VulkanImage vk_image = Vulkan_CreateImage_RecordCmds(
          command_buffer
        , &upload_buffer
        , &upload_buffer_memory
        , device
        , render.render_data_.physical_device
        , format
        , width
        , height
        , ptr);

    KK_VERIFY(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    KK_VERIFY(vkQueueSubmit(render.render_data_.work_queue, 1, &end_info, VK_NULL_HANDLE) == VK_SUCCESS);

    render.defer_clean_up([=, &render, image_ref = ref]() mutable
    {
        vkFreeMemory(device, upload_buffer_memory, nullptr);
        vkDestroyBuffer(device, upload_buffer, nullptr);
        vkFreeCommandBuffers(device, render.render_data_.work_command_pool, 1, &command_buffer);
        image_ref = ImageRef();
    });

    return vk_image;
}

/*static*/ ImageRef ImageRef::FromMemory(KidsRender& render
    , Format format
    , int width
    , int height
    , const void* data)
{
    ImageRef image_ref;
    image_ref.ref_ = ImageState::New();
    image_ref.ref_->width_ = width;
    image_ref.ref_->height_ = height;
    image_ref.ref_->device_ = render.render_data_.device;

    VulkanImage vk_image = Vulkan_CreateImageTexture(image_ref
        , render
        , format
        , width
        , height
        , data);

    image_ref.ref_->memory_ = vk_image.memory_;
    image_ref.ref_->image_ = vk_image.image_;
    image_ref.ref_->image_view_ = vk_image.image_view_;

    return image_ref;
}

ImageRef::ImageState::~ImageState() noexcept
{
    vkFreeMemory(device_, memory_, nullptr);
    vkDestroyImage(device_, image_, nullptr);
    vkDestroyImageView(device_, image_view_, nullptr);
}
#endif
} // namespace kr
