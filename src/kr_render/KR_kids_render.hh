#pragma once
#include "KR_kids_config.hh"
#include "KR_kids_image.hh"

#include <functional>
#include <vector>
#include <span>
#include <cstdint>
#include <climits>

namespace kr
{

struct DrawCmd
{
    ClipRect clip_rect_{};
    ImageRef texture_{};
    unsigned vertex_offset_ = 0;
    unsigned vertex_count_ = 0;
    unsigned index_offset_ = 0;
    unsigned index_count_ = 0;
    kk::Vec2f scale_{1.f, 1.f};
};

struct Vertex
{
    kk::Vec2f p_;
    kk::Vec2f uv_;
    kk::Vec4f c_;
};

using Index = std::uint16_t;

struct CmdList
{
    std::vector<DrawCmd> draw_list_;
    std::vector<Vertex> vertex_list_;
    std::vector<Index> index_list_;
};

#if (KK_RENDER_OPENGL())
struct RenderData
{
};
struct FrameInfo
{
    kk::Size screen_size;
};
#endif
#if (KK_RENDER_VULKAN())
struct RenderData
{
    VkPhysicalDevice physical_device{};
    VkDevice device{};
    VkQueue work_queue{};
    VkCommandPool work_command_pool{};
    VkDescriptorPool descriptor_pool{};
    VkRenderPass render_pass{};
    std::uint32_t image_count{};
    VkSampleCountFlagBits msaa_samples{};
    std::function<std::uint32_t ()> current_frame_;
};
struct FrameInfo
{
    kk::Size screen_size{};
    VkCommandBuffer command_buffer{};
    std::uint32_t frame_index{};
};
#endif

struct KidsRender
{
public:
    KidsRender();
    ~KidsRender() noexcept;
    KidsRender(KidsRender&& rhs) noexcept = delete;
    KidsRender(const KidsRender&) noexcept = delete;
    KidsRender& operator=(const KidsRender&) noexcept = delete;
    KidsRender& operator=(KidsRender&&) noexcept = delete;

public:
    static void Build(const RenderData&, KidsRender& render);

public:
    void line(const kk::Point2f& p1
        , const kk::Point2f& p2
        , const kk::Color& color = kk::Color_White()
        , float width = 1.f
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    // Counterclockwise?
    void triangle(const kk::Point2f& p1
        , const kk::Point2f& p2
        , const kk::Point2f& p3
        , const kk::Color& color = kk::Color_White()
        , float width = 1.f
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void triangle_fill(const kk::Point2f& p1
        , const kk::Point2f& p2
        , const kk::Point2f& p3
        , const kk::Color& color = kk::Color_White()
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void rect(const kk::Point2f& p_min
        , const kk::Point2f& p_max
        , const kk::Color& color = kk::Color_White()
        , float width = 1.f
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void rect_fill(const kk::Point2f& p_min
        , const kk::Point2f& p_max
        , const kk::Color& color = kk::Color_White()
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void circle(const kk::Point2f& p_center
        , float radius
        , const kk::Color& color = kk::Color_White()
        , float width = 1.f
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , int segments_count = -1
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void circle_fill(const kk::Point2f& p_center
        , float radius
        , const kk::Color& color = kk::Color_White()
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , int segments_count = -1
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void image(const ImageRef& image
        , const kk::Point2f& p_min = kk::Point2f{0.f, 0.f}
        , const kk::Point2f& p_max = kk::Point2f_Invalid() // Use image's size.
        , const kk::Vec2f& uv_min = kk::Vec2f{0.f, 0.f}  // Use full image to sample.
        , const kk::Vec2f& uv_max = kk::Vec2f{1.f, 1.f}  // 
        , const kk::Color& color = kk::Color_White()
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void bezier_cubic(
          const kk::Point2f& p1
        , const kk::Point2f& p2
        , const kk::Point2f& p3
        , const kk::Point2f& p4
        , const kk::Color& color = kk::Color_White()
        , float width = 1.f
        , int segments_count = -1
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);
    void bezier_quadratic(
          const kk::Point2f& p1
        , const kk::Point2f& p2
        , const kk::Point2f& p3
        , const kk::Color& color = kk::Color_White()
        , float width = 1.f
        , const kk::Vec2f& scale = kk::Vec2f{1.f, 1.f}
        , int segments_count = -1
        , const ClipRect& clip_rect = {}
        , CmdList* cmd_list = nullptr);

public:
    void draw(const FrameInfo& frame);
    void clear();

    // Lowest-level API: push whatever makes sense.
    void merge_cmd_lists(const CmdList& new_cmd_list
        , const kk::Point2f* translate_by = nullptr
        , const ClipRect* override_clip_rect = nullptr
        , CmdList* append_to_cmd_list = nullptr);

// private:
    RenderData render_data_;
    CmdList cmd_list_;
    ImageRef white_1x1_;

#if (KK_RENDER_OPENGL())
    unsigned int shader_program_ = 0;
    int screen_width_ptr_ = -1;
    int screen_height_ptr_ = -1;
    int scale_x_ptr_ = -1;
    int scale_y_ptr_ = -1;
    int texture_ptr_ = -1;
#endif
#if (KK_RENDER_VULKAN())
    struct Vulkan_Pipeline
    {
        VkPipelineLayout layout{};
        VkPipeline pipeline{};
    };
    struct Vulkan_Frame
    {
        VkBuffer vertex_buffer{};
        VkDeviceMemory vertex_memory{};
        VkBuffer index_buffer{};
        VkDeviceMemory index_memory{};
        std::vector<VkDescriptorSet> descriptor_set_list_{};
        std::vector<ImageRef> image_in_use_list_;

        std::vector<std::function<void ()>> to_flush_;
        std::vector<std::function<void ()>> clean_up_list_;
    };
    // Owns.
    Vulkan_Pipeline triangle_pipeline_{};
    std::vector<Vulkan_Frame> frame_list_{};
    VkSampler texture_sampler_{};
    VkDescriptorSetLayout descriptor_set_layout_{};
    VkDescriptorSet white_1x1_descriptor_set_;

    void defer_clean_up(std::function<void ()> f);
#endif

private:
    static void AddVertices(CmdList& cmd_list
        , const ImageRef& texture
        , const std::span<const Vertex>& new_vertices
        , const std::span<const Index>& new_indices
        , const ClipRect& clip_rect
        , const kk::Vec2f& scale);
    void circle_impl(const kk::Point2f& p_center
        , float radius
        , bool do_fill
        , const kk::Color& color
        , int segments_count
        , const ClipRect& clip_rect
        , CmdList* cmd_list
        , const kk::Vec2f& scale
        , float width);
};

} // namespace kr
