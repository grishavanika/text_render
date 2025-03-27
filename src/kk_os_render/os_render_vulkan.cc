// https://vulkan-tutorial.com.
// 
#include "os_render_backend.hh"
#include "os_window.hh"
#include "os_window_UTILS.hh"

#include <vector>
#include <algorithm>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
#  define NOMINMAX
#  include <Windows.h>
#  include <tchar.h>
#endif

#include <vulkan/vulkan.h>

#if (_MSC_VER)
#  include <vulkan/vulkan_win32.h>
#endif

#if (KK_WINDOW_GLFW())
#  define GLFW_INCLUDE_NONE
#  define GLFW_INCLUDE_VULKAN
#  include <GLFW/glfw3.h>
#endif

#if (!KK_WINDOW_VULKAN())
#  error OpenGL Render requires Window with Vulkan context (KK_WINDOW_VULKAN() == 1).
#endif

static float N_(std::uint8_t c)
{
    return (c / 255.f);
}

static bool Str_StartsWith(const char* str, const char* str_begin)
{
    const size_t len1 = strlen(str);
    const size_t len2 = strlen(str_begin);
    return (strncmp(str, str_begin, (std::min)(len1, len2)) == 0);
}

static VkBool32 vkDebugUtilsMessengerCallbackEXT_(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/)
{
#if (1)
    if ((messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
        || (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT))
        return VK_FALSE;
    // [Error]: loader_get_json: Failed to open JSON file SteamOverlayVulkanLayer64.json
    if ((messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        && Str_StartsWith(pCallbackData->pMessage, "loader_get_json"))
        return VK_FALSE;
#endif

    const char* const severity = [&]
    {
        switch (messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "Verbose";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT   : return "Info";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "Warning";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT  : return "Error";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: return "";
        }
        return "";
    }();
    fprintf(stderr, "[%s]: %s\n", severity, pCallbackData->pMessage);
    fflush(stderr);
    return VK_FALSE;
}

static void Panic_(bool condition, const char* file, unsigned line)
{
    if (!condition)
    {
        fprintf(stderr, "Panic. %s,%u.\n", file, line);
        fflush(stderr);
        abort();
    }
}

static void Panic_(VkResult vk, const char* file, unsigned line)
{
    if (vk != VK_SUCCESS)
    {
        fprintf(stderr, "Panic. VK: %#x. %s,%u.\n", unsigned(vk), file, line);
        fflush(stderr);
        abort();
    }
}

#define Panic(X) Panic_((X), __FILE__, __LINE__)

static const char* const kEnabledLogicalExtensions[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

static const char* const kEnabledLayers[] =
{
    "VK_LAYER_KHRONOS_validation",
};

static const char* const kEnabledPhysExtensions[] =
{
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,   // Debug.
    VK_KHR_SURFACE_EXTENSION_NAME,
#if (_MSC_VER)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
};

#define KK_REQUIRE(C) Panic(C)

const std::size_t k_MaxFramesInFlight = 2;

struct VulkanWindow
{
    VkSurfaceKHR surface_{};
    VkSwapchainKHR swapchain_{};
    std::vector<VkImage> swapchain_images_{};
    std::vector<VkImageView> swapchain_image_views_{};
    VkFormat image_format_{};
    VkExtent2D image_extent_{uint32_t(-1), uint32_t(-1)};
    VkRenderPass render_pass_{};
    std::vector<VkFramebuffer> framebuffers_{};
    std::vector<VkCommandBuffer> command_buffers{};

    std::vector<VkSemaphore> semaphores_image_available{};
    std::vector<VkSemaphore> semaphores_render_finished{};
    std::vector<VkFence> in_flight_fences{};

    uint32_t current_frame = 0;
    uint32_t image_index = 0;
    int old_width = 0;
    int old_height = 0;

    VkImage msaa_color_image_{};
    VkSampleCountFlagBits msaa_samples_ = VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
    VkDeviceMemory msaa_color_image_memory_{};
    VkImageView msaa_color_image_view_;

    OsWindow* window_ = nullptr;

    void create_swap_chain(VkPhysicalDevice physical_device, VkDevice device)
    {
        Panic(!swapchain_);
        Panic(image_format_ == VK_FORMAT_UNDEFINED);
        Panic(image_extent_.width == uint32_t(-1));
        Panic(swapchain_images_.size() == 0);
        Panic(surface_);
        Panic(physical_device);
        Panic(device);
        // Panic(msaa_samples_ == VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);
        msaa_samples_ = max_sample_count(physical_device);

        // For the color space we'll use SRGB if it is available, because it results in more
        // accurate perceived colors.It is also pretty much the standard color space
        // for images, like the textures we'll use later on.
        uint32_t format_count = 0;
        Panic(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &format_count, nullptr));
        std::vector<VkSurfaceFormatKHR> formats{std::size_t(format_count)};
        Panic(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface_, &format_count, formats.data()));
        Panic(formats.size() > 0);

        // Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available.
        uint32_t present_mode_count = 0;
        Panic(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface_, &present_mode_count, nullptr));
        std::vector<VkPresentModeKHR> present_modes{std::size_t(present_mode_count)};
        Panic(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface_, &present_mode_count, present_modes.data()));
        Panic(present_modes.size() > 0);

        VkSurfaceCapabilitiesKHR capabilities{};
        Panic(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface_, &capabilities));
        Panic(capabilities.currentExtent.height != UINT32_MAX);

        // Simply sticking to this minimum means that we may sometimes have
        // to wait on the driver to complete internal operations before we can acquire
        // another image to render to.Therefore it is recommended to request at least one
        // more image than the minimum.
        const uint32_t images_count = std::clamp(capabilities.minImageCount + 1,
            capabilities.minImageCount,
            (std::max)(capabilities.maxImageCount, capabilities.minImageCount));

        VkSwapchainCreateInfoKHR swapchain_info{};
        swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_info.pNext = nullptr;
        swapchain_info.flags = 0;
        swapchain_info.surface = surface_;
        swapchain_info.minImageCount = images_count;
        swapchain_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchain_info.imageExtent = capabilities.currentExtent;
        // Amount of layers each image consists of.
        // This is always 1 unless you are developing a stereoscopic 3D application.
        swapchain_info.imageArrayLayers = 1;
        // We're going to render directly to them, which
        // means that they're used as color attachment.It is also possible that you'll render
        // images to a separate image first to perform operations like post - processing.In
        // that case you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT.
        swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // 1 queue for present & graphics.
        swapchain_info.queueFamilyIndexCount = 0; // Optional.
        swapchain_info.pQueueFamilyIndices = nullptr; // Optional.
        swapchain_info.preTransform = capabilities.currentTransform;
        swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchain_info.clipped = VK_TRUE;
        swapchain_info.oldSwapchain = VK_NULL_HANDLE;

        Panic(vkCreateSwapchainKHR(device, &swapchain_info, nullptr/*Allocator*/, &swapchain_));

        uint32_t actual_images_count = 0;
        Panic(vkGetSwapchainImagesKHR(device, swapchain_, &actual_images_count, nullptr));
        swapchain_images_.resize(actual_images_count);
        Panic(vkGetSwapchainImagesKHR(device, swapchain_, &actual_images_count, swapchain_images_.data()));

        image_extent_ = swapchain_info.imageExtent;
        image_format_ = swapchain_info.imageFormat;
    }

    void create_swap_chain_image_views(VkDevice device)
    {
        Panic(swapchain_image_views_.size() == 0);
        Panic(swapchain_images_.size() > 0);
        Panic(image_format_ != VkFormat::VK_FORMAT_UNDEFINED);

        swapchain_image_views_.reserve(swapchain_images_.size());
        for (VkImage image : swapchain_images_)
        {
            VkImageViewCreateInfo image_view_info{};
            image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            image_view_info.pNext = nullptr;
            image_view_info.flags = 0;
            image_view_info.image = image;
            image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            image_view_info.format = image_format_;
            image_view_info.components = {
                  VK_COMPONENT_SWIZZLE_IDENTITY
                , VK_COMPONENT_SWIZZLE_IDENTITY
                , VK_COMPONENT_SWIZZLE_IDENTITY
                , VK_COMPONENT_SWIZZLE_IDENTITY};
            // Our images will be used as color targets
            // without any mipmapping levels or multiple layers.
            image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            image_view_info.subresourceRange.baseMipLevel = 0;
            image_view_info.subresourceRange.levelCount = 1;
            image_view_info.subresourceRange.baseArrayLayer = 0;
            image_view_info.subresourceRange.layerCount = 1;

            swapchain_image_views_.push_back(VK_NULL_HANDLE);
            Panic(vkCreateImageView(device
                , &image_view_info
                , nullptr/*Allocator*/
                , &swapchain_image_views_.back()));
        }
    }

    void create_render_pass(VkDevice device)
    {
        Panic(!render_pass_);
        Panic(image_format_ != VkFormat::VK_FORMAT_UNDEFINED);
        Panic(device);
        Panic(msaa_samples_ != VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);


        VkAttachmentDescription color_attachment{};
        color_attachment.flags = 0;
        color_attachment.format = image_format_;
        color_attachment.samples = msaa_samples_;
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription color_attachment_resolve{};
        color_attachment_resolve.format = image_format_;
        color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
        color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment_resolve_ref{};
        color_attachment_resolve_ref.attachment = 1;
        color_attachment_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment_ref{};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.flags = 0;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.inputAttachmentCount = 0;
        subpass.pInputAttachments = nullptr;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pResolveAttachments = &color_attachment_resolve_ref;
        subpass.pDepthStencilAttachment = nullptr;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = nullptr;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = 0;

        VkAttachmentDescription attachments[] = {color_attachment, color_attachment_resolve};
        VkRenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.pNext = nullptr;
        render_pass_info.flags = 0;
        render_pass_info.attachmentCount = uint32_t(std::size(attachments));
        render_pass_info.pAttachments = attachments;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        Panic(vkCreateRenderPass(device, &render_pass_info, nullptr/*Allocator*/, &render_pass_));
    }

    void create_color_resources(VkPhysicalDevice physical_device, VkDevice device)
    {
        Panic(msaa_samples_ != VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = image_extent_.width;
        imageInfo.extent.height = image_extent_.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = image_format_;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = msaa_samples_;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        Panic(vkCreateImage(device, &imageInfo, nullptr, &msaa_color_image_));

        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkMemoryRequirements mem_requirements;
        vkGetImageMemoryRequirements(device, msaa_color_image_, &mem_requirements);
        VkPhysicalDeviceMemoryProperties mem_properties{};
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
        uint32_t mem_index = uint32_t(-1);
        for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i)
        {
            if ((mem_requirements.memoryTypeBits & (1 << i))
                && ((mem_properties.memoryTypes[i].propertyFlags & properties) == properties))
            {
                mem_index = i;
                break;
            }
        }
        Panic(mem_index != uint32_t(-1));

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = mem_requirements.size;
        allocInfo.memoryTypeIndex = mem_index;

        Panic(vkAllocateMemory(device, &allocInfo, nullptr, &msaa_color_image_memory_));
        Panic(vkBindImageMemory(device, msaa_color_image_, msaa_color_image_memory_, 0));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = msaa_color_image_;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = image_format_;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        Panic(vkCreateImageView(device, &viewInfo, nullptr, &msaa_color_image_view_));
    }

    void create_frame_buffers(VkDevice device)
    {
        Panic(framebuffers_.size() == 0);
        Panic(render_pass_);
        Panic(image_extent_.width != uint32_t(-1));

        framebuffers_.reserve(swapchain_image_views_.size());
        for (VkImageView& view : swapchain_image_views_)
        {
            VkImageView attachments[] = {msaa_color_image_view_, view};
            VkFramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebuffer_info.pNext = nullptr;
            framebuffer_info.flags = 0;
            framebuffer_info.renderPass = render_pass_;
            framebuffer_info.attachmentCount = uint32_t(std::size(attachments));
            framebuffer_info.pAttachments = attachments;
            framebuffer_info.width = image_extent_.width;
            framebuffer_info.height = image_extent_.height;
            framebuffer_info.layers = 1;

            framebuffers_.push_back(VK_NULL_HANDLE);
            Panic(vkCreateFramebuffer(device
                , &framebuffer_info
                , nullptr
                , &framebuffers_.back()));
        }
    }

    void cleanup_swap_chain(VkDevice device)
    {
        KK_REQUIRE(device);
        for (VkFramebuffer& framebuffer : framebuffers_)
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        vkDestroyRenderPass(device, render_pass_, nullptr);

        vkDestroyImageView(device, msaa_color_image_view_, nullptr);
        vkDestroyImage(device, msaa_color_image_, nullptr);
        vkFreeMemory(device, msaa_color_image_memory_, nullptr);

        for (VkImageView& view : swapchain_image_views_)
            vkDestroyImageView(device, view, nullptr);

        vkDestroySwapchainKHR(device, swapchain_, nullptr);

        image_format_ = VK_FORMAT_UNDEFINED;
        image_extent_.width = uint32_t(-1);
        framebuffers_.clear();
        render_pass_ = {};
        swapchain_image_views_.clear();
        swapchain_images_.clear();
        swapchain_ = {};
    }

    void recreate_swap_chain(OsWindow&, VkPhysicalDevice physical_device, VkDevice device)
    {
        KK_REQUIRE(device);

        vkDeviceWaitIdle(device);

        cleanup_swap_chain(device);

        create_swap_chain(physical_device, device);
        create_swap_chain_image_views(device);
        create_render_pass(device);
        create_color_resources(physical_device, device);
        create_frame_buffers(device);
    }

    VkSampleCountFlagBits max_sample_count(VkPhysicalDevice physical_device)
    {
        VkPhysicalDeviceProperties p{};
        vkGetPhysicalDeviceProperties(physical_device, &p);

        VkSampleCountFlags counts = p.limits.framebufferColorSampleCounts & p.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
        return VK_SAMPLE_COUNT_1_BIT;
    }

    void create_from_os_window(OsWindow& window
        , VkInstance vk_instance
        , VkPhysicalDevice physical_device
        , VkDevice device
        , VkCommandPool command_pool)
    {
        Panic(!window_);
        window_ = &window;

#if (KK_WINDOW_WIN32())
        VkWin32SurfaceCreateInfoKHR surface_info{};
        surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surface_info.pNext = nullptr;
        surface_info.flags = 0;
        surface_info.hinstance = ::GetModuleHandle(nullptr);
        surface_info.hwnd = (HWND)window.context.hwnd;
        Panic(vkCreateWin32SurfaceKHR(vk_instance, &surface_info, nullptr/*Allocator*/, &surface_));
#elif (KK_WINDOW_GLFW())
        GLFWwindow* glfw_window = window.context.glfw_wnd;
        Panic(glfwCreateWindowSurface(vk_instance, glfw_window, nullptr, &surface_));
#else
#  error Unknown Window backend.
#endif
        create_swap_chain(physical_device, device);
        create_swap_chain_image_views(device);
        create_render_pass(device);
        create_color_resources(physical_device, device);
        create_frame_buffers(device);

        command_buffers.resize(framebuffers_.size());

        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.pNext = nullptr;
        alloc_info.commandPool = command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = uint32_t(command_buffers.size());
        Panic(vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data()));

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore_info.pNext = nullptr;
        semaphore_info.flags = 0;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.pNext = nullptr;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        semaphores_image_available.resize(k_MaxFramesInFlight);
        semaphores_render_finished.resize(k_MaxFramesInFlight);
        in_flight_fences.resize(k_MaxFramesInFlight);
        for (std::size_t i = 0; i < k_MaxFramesInFlight; ++i)
        {
            Panic(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphores_image_available[i]));
            Panic(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphores_render_finished[i]));
            Panic(vkCreateFence(device, &fence_info, nullptr, &in_flight_fences[i]));
        }

        const kk::Size wnd_size = Wnd_Size(window);
        old_width = wnd_size.width;
        old_height = wnd_size.height;
    }

    void cleanup(VkInstance vk_instance, VkDevice device)
    {
        cleanup_swap_chain(device);
        for (VkSemaphore& x : semaphores_render_finished)
            vkDestroySemaphore(device, x, nullptr);
        for (VkSemaphore& x : semaphores_image_available)
            vkDestroySemaphore(device, x, nullptr);
        for (VkFence& x : in_flight_fences)
            vkDestroyFence(device, x, nullptr);
        vkDestroySurfaceKHR(vk_instance, surface_, nullptr);
        semaphores_render_finished.clear();
        semaphores_image_available.clear();
        in_flight_fences.clear();
    }
};

struct VulkanState
{
    VkInstance vk_instance_{};
    std::vector<VulkanWindow> all_windows_{};
    VkPhysicalDevice physical_device_{};
    uint32_t graphics_family_index_{uint32_t(-1)};
    VkDevice device_{};
    VkQueue graphics_queue_{};

    VkDescriptorPool descriptor_pool{};
    VkCommandPool command_pool{};
    VkDebugUtilsMessengerEXT vk_debug_msg{};

    void pick_GPU()
    {
        Panic(!physical_device_);
        KK_REQUIRE(vk_instance_);

        uint32_t device_count = 0;
        Panic(vkEnumeratePhysicalDevices(vk_instance_, &device_count, nullptr));
        Panic(device_count > 0);
        std::vector<VkPhysicalDevice> devices{std::size_t(device_count)};
        Panic(vkEnumeratePhysicalDevices(vk_instance_, &device_count, devices.data()));

        auto it = std::find_if(std::begin(devices), std::end(devices)
            , [](VkPhysicalDevice physical_device)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physical_device, &properties);
            return (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
        });
        if (it != std::end(devices))
            physical_device_ = *it;
        else
            physical_device_ = devices.front();
    }

    void create_logical_device()
    {
        Panic(!device_);
        Panic(!graphics_queue_);
        Panic(graphics_family_index_ == uint32_t(-1));
        Panic(physical_device_);

        uint32_t family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &family_count, nullptr);
        std::vector<VkQueueFamilyProperties> families{std::size_t(family_count)};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &family_count, families.data());
        auto graphics_it = std::find_if(std::begin(families), std::end(families)
            , [](const VkQueueFamilyProperties& family_info)
        {
            return (family_info.queueFlags & VK_QUEUE_GRAPHICS_BIT);
        });
        Panic(graphics_it != std::end(families));
        graphics_family_index_ = uint32_t(std::distance(std::begin(families), graphics_it));

#if (0)
        // To simplify things.
        VkBool32 present_support = false;
        Panic(vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device_, graphics_family_index_, WINDOW.surface_, &present_support));
        Panic(present_support);
#endif
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.pNext = nullptr;
        queue_info.flags = 0;
        queue_info.queueFamilyIndex = graphics_family_index_;
        queue_info.queueCount = 1;
        const float priority = 1.f;
        queue_info.pQueuePriorities = &priority;

        VkPhysicalDeviceFeatures device_features{};

        VkDeviceCreateInfo device_info{};
        device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_info.pNext = nullptr;
        device_info.flags = 0;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        device_info.enabledLayerCount = uint32_t(std::size(kEnabledLayers)); // Ignored.
        device_info.ppEnabledLayerNames = kEnabledLayers; // Ignored.
        device_info.enabledExtensionCount = uint32_t(std::size(kEnabledLogicalExtensions));
        device_info.ppEnabledExtensionNames = kEnabledLogicalExtensions;
        device_info.pEnabledFeatures = &device_features;

        Panic(vkCreateDevice(physical_device_, &device_info, nullptr/*Allocator*/, &device_));

        vkGetDeviceQueue(device_, graphics_family_index_, 0, &graphics_queue_);
    }

    VulkanWindow& VulkanWindow_FromOsWindow(OsWindow& window)
    {
        for (VulkanWindow& vw : all_windows_)
        {
            if (vw.window_ == &window)
                return vw;
        }
        Panic(false);
        static VulkanWindow xx;
        return xx;
    }
};

OsRender_State* OsRender_Create()
{
    OsRender_State* void_state = new VulkanState{};
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "vulkan playground";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "playground";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkDebugUtilsMessengerCreateInfoEXT debug_info{};
    debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_info.pNext = nullptr;
    debug_info.flags = 0;
    debug_info.messageSeverity =
          VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_info.messageType =
          VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_info.pfnUserCallback = &vkDebugUtilsMessengerCallbackEXT_;
    debug_info.pUserData = nullptr;

    std::vector<const char*> extension_list;
    auto merge_to_extension_list = [&extension_list](const char* const* list, uint32_t size)
    {
        for (uint32_t i = 0; i < size; ++i)
        {
            const char* const new_ = list[i];
            bool should_add = true;
            for (const char* existing : extension_list)
            {
                if (strcmp(existing, new_) == 0)
                {
                    should_add = false;
                    break;
                }
            }
            if (should_add)
                extension_list.push_back(new_);
        }
    };
#if (KK_WINDOW_GLFW())
    uint32_t count = 0;
    const char** const GLFW_extensions = ::glfwGetRequiredInstanceExtensions(&count);
    merge_to_extension_list(GLFW_extensions, count);
#endif
    merge_to_extension_list(kEnabledPhysExtensions, uint32_t(std::size(kEnabledPhysExtensions)));
    Panic(extension_list.size() > 0);

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = &debug_info;
    create_info.flags = 0;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = uint32_t(std::size(kEnabledLayers));
    create_info.ppEnabledLayerNames = kEnabledLayers;
    create_info.enabledExtensionCount = uint32_t(extension_list.size());
    create_info.ppEnabledExtensionNames = extension_list.data();

    Panic(vkCreateInstance(&create_info, nullptr/*Allocator*/, &vulkan_app.vk_instance_));

    auto vkCreateDebugUtilsMessengerEXT_ = PFN_vkCreateDebugUtilsMessengerEXT(vkGetInstanceProcAddr(
        vulkan_app.vk_instance_, "vkCreateDebugUtilsMessengerEXT"));
    Panic(vkCreateDebugUtilsMessengerEXT_(vulkan_app.vk_instance_, &debug_info, nullptr/*Allocator*/, &vulkan_app.vk_debug_msg));

    vulkan_app.pick_GPU();
    vulkan_app.create_logical_device();
    
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.pNext = nullptr;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = vulkan_app.graphics_family_index_;
    
    Panic(vkCreateCommandPool(vulkan_app.device_, &pool_info, nullptr, &vulkan_app.command_pool));

    VkDescriptorPoolSize descriptor_pool_size[] =
    {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024},
    };
    VkDescriptorPoolCreateInfo descriptor_pool_info{};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.pNext = nullptr;
    descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptor_pool_info.poolSizeCount = static_cast<uint32_t>(std::size(descriptor_pool_size));
    descriptor_pool_info.maxSets = 1024;
    descriptor_pool_info.pPoolSizes = descriptor_pool_size;
    Panic(vkCreateDescriptorPool(vulkan_app.device_, &descriptor_pool_info, nullptr, &vulkan_app.descriptor_pool));

    return void_state;
}

void OsRender_WindowCreate(OsRender_State* void_state, OsWindow& window)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
    vulkan_app.all_windows_.push_back({});
    VulkanWindow& vulkan_w = vulkan_app.all_windows_.back();
    vulkan_w.create_from_os_window(window
        , vulkan_app.vk_instance_
        , vulkan_app.physical_device_
        , vulkan_app.device_
        , vulkan_app.command_pool);
}

void OsRender_WindowDestroy(OsRender_State* void_state, OsWindow& window)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
    auto it = std::find_if(vulkan_app.all_windows_.begin(), vulkan_app.all_windows_.end()
        , [&window](VulkanWindow& vw)
    {
        return (vw.window_ == &window);
    });
    Panic(it != vulkan_app.all_windows_.end());
    it->window_ = nullptr; // mark deleted

    // #TODO: delete later.
    Panic(vkDeviceWaitIdle(vulkan_app.device_));
    it->cleanup(vulkan_app.vk_instance_, vulkan_app.device_);
    vulkan_app.all_windows_.erase(it);
}

void OsRender_Build(OsRender_State* void_state, OsWindow& window, kr::KidsRender& render)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
    VulkanWindow& vulkan_w = vulkan_app.VulkanWindow_FromOsWindow(window);

    kr::RenderData render_data;
    render_data.physical_device = vulkan_app.physical_device_;
    render_data.device = vulkan_app.device_;
    render_data.render_pass = vulkan_w.render_pass_;
    render_data.image_count = std::uint32_t(vulkan_w.framebuffers_.size());
    render_data.msaa_samples = vulkan_w.msaa_samples_;
    render_data.work_queue = vulkan_app.graphics_queue_;
    render_data.work_command_pool = vulkan_app.command_pool;
    render_data.descriptor_pool = vulkan_app.descriptor_pool;
    render_data.current_frame_ = [void_state, &window]()
    {
        VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
        VulkanWindow& vulkan_w = vulkan_app.VulkanWindow_FromOsWindow(window);
        return vulkan_w.current_frame;
    };
    kr::KidsRender::Build(render_data, render);
}

void OsRender_NewFrame(OsRender_State* void_state
    , OsWindow& window
    , const kk::Color& clear_color
    , OsRender_NewFrameCallback render_frame_callback)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
    VulkanWindow& vulkan_w = vulkan_app.VulkanWindow_FromOsWindow(window);

    Panic(vkWaitForFences(vulkan_app.device_, 1, &vulkan_w.in_flight_fences[vulkan_w.current_frame], VK_TRUE, UINT64_MAX));

    vulkan_w.image_index = 0;
    const VkResult ok = vkAcquireNextImageKHR(vulkan_app.device_
        , vulkan_w.swapchain_
        , UINT64_MAX
        , vulkan_w.semaphores_image_available[vulkan_w.current_frame]
        , VK_NULL_HANDLE
        , &vulkan_w.image_index);
    switch (ok)
    {
    case VK_ERROR_OUT_OF_DATE_KHR:
        if (Wnd_IsAlive(window)) // When closed.
            vulkan_w.recreate_swap_chain(window, vulkan_app.physical_device_, vulkan_app.device_);
        return; // ??
    case VK_SUBOPTIMAL_KHR:
        break;
    default:
        Panic(ok);
    }
    Panic(vkResetFences(vulkan_app.device_, 1, &vulkan_w.in_flight_fences[vulkan_w.current_frame]));

    // Record command buffer.
    VkCommandBuffer cmd_buffer = vulkan_w.command_buffers[vulkan_w.current_frame];
    Panic(vkResetCommandBuffer(cmd_buffer, 0));

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = nullptr;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.pNext = nullptr;
    render_pass_begin_info.renderPass = vulkan_w.render_pass_;
    render_pass_begin_info.framebuffer = vulkan_w.framebuffers_[vulkan_w.image_index];
    render_pass_begin_info.renderArea.extent = vulkan_w.image_extent_;
    render_pass_begin_info.renderArea.offset = VkOffset2D{0, 0};
    VkClearValue vk_clear_color{{{N_(clear_color.r), N_(clear_color.g), N_(clear_color.b), N_(clear_color.a)}}};
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &vk_clear_color;

    Panic(vkBeginCommandBuffer(cmd_buffer, &begin_info));
    vkCmdBeginRenderPass(cmd_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    const kk::Size wnd_size = Wnd_Size(window);
    kr::FrameInfo frame_info;
    frame_info.screen_size = wnd_size;
    frame_info.command_buffer = cmd_buffer;
    frame_info.frame_index = vulkan_w.current_frame;
    render_frame_callback(window, frame_info);

    vkCmdEndRenderPass(cmd_buffer);
    Panic(vkEndCommandBuffer(cmd_buffer));
}

void OsRender_Present(OsRender_State* void_state, OsWindow& window)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
    VulkanWindow& vulkan_w = vulkan_app.VulkanWindow_FromOsWindow(window);

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &vulkan_w.semaphores_image_available[vulkan_w.current_frame];
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &vulkan_w.command_buffers[vulkan_w.current_frame];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &vulkan_w.semaphores_render_finished[vulkan_w.current_frame];
    Panic(vkQueueSubmit(vulkan_app.graphics_queue_, 1, &submit_info, vulkan_w.in_flight_fences[vulkan_w.current_frame]));

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = nullptr;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &vulkan_w.semaphores_render_finished[vulkan_w.current_frame];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &vulkan_w.swapchain_;
    present_info.pImageIndices = &vulkan_w.image_index;
    present_info.pResults = nullptr;

    const kk::Size wnd_size = Wnd_Size(window);
    const bool resize = ((wnd_size.width != vulkan_w.old_width) || (wnd_size.height != vulkan_w.old_height));
    vulkan_w.old_width = wnd_size.width;
    vulkan_w.old_height = wnd_size.height;

    const VkResult present_status = vkQueuePresentKHR(vulkan_app.graphics_queue_, &present_info);
    bool recreate = resize;
    switch (present_status)
    {
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
        recreate = true;
        break;
    default:
        Panic(present_status);
    }
    if (!Wnd_IsAlive(window)) // When closed.
        recreate = false;
    if (recreate)
        vulkan_w.recreate_swap_chain(window, vulkan_app.physical_device_, vulkan_app.device_);

    vulkan_w.current_frame = ((vulkan_w.current_frame + 1) % k_MaxFramesInFlight);
}

void OsRender_Finish(OsRender_State* void_state)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);
    Panic(vkDeviceWaitIdle(vulkan_app.device_));
}

void OsRender_Destroy(OsRender_State* void_state)
{
    VulkanState& vulkan_app = *static_cast<VulkanState*>(void_state);

    for (VulkanWindow& vulkan_w : vulkan_app.all_windows_)
        vulkan_w.cleanup(vulkan_app.vk_instance_, vulkan_app.device_);
    vulkan_app.all_windows_.clear();
    vkDestroyDescriptorPool(vulkan_app.device_, vulkan_app.descriptor_pool, nullptr);
    vkDestroyCommandPool(vulkan_app.device_, vulkan_app.command_pool, nullptr);
    vkDestroyDevice(vulkan_app.device_, nullptr);
    auto vkDestroyDebugUtilsMessengerEXT_ = PFN_vkDestroyDebugUtilsMessengerEXT(vkGetInstanceProcAddr(
        vulkan_app.vk_instance_, "vkDestroyDebugUtilsMessengerEXT"));
    vkDestroyDebugUtilsMessengerEXT_(vulkan_app.vk_instance_, vulkan_app.vk_debug_msg, nullptr);
    vkDestroyInstance(vulkan_app.vk_instance_, nullptr);

    delete &vulkan_app;
}
