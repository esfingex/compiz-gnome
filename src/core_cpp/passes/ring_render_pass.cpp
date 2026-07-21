#include "ring_render_pass.hpp"

namespace compiz::core::passes {

RingRenderPass::RingRenderPass(vulkan::VulkanContext& ctx, VkExtent2D extent)
    : ctx_(ctx), extent_(extent) {
    pc_.total_angle = 0.0f;
    pc_.radius = 0.5f;
    pc_.tilt_angle = 0.3f;
    pc_.scale_factor = 1.0f;
    pc_.spacing = 0.8f;
    pc_.window_count = 0;
}

RingRenderPass::~RingRenderPass() {
    cleanup();
}

bool RingRenderPass::init() {
    create_pipeline();
    return true;
}

void RingRenderPass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    if (pipeline_)        { vkDestroyPipeline(dev, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (pl_layout_)       { vkDestroyPipelineLayout(dev, pl_layout_, nullptr); pl_layout_ = VK_NULL_HANDLE; }
    if (ds_layout_)       { vkDestroyDescriptorSetLayout(dev, ds_layout_, nullptr); ds_layout_ = VK_NULL_HANDLE; }
    if (descriptor_pool_){ vkDestroyDescriptorPool(dev, descriptor_pool_, nullptr); descriptor_pool_ = VK_NULL_HANDLE; }
    if (instance_buffer_){ vkDestroyBuffer(dev, instance_buffer_, nullptr); instance_buffer_ = VK_NULL_HANDLE; }
    if (instance_memory_){ vkFreeMemory(dev, instance_memory_, nullptr); instance_memory_ = VK_NULL_HANDLE; }
}

void RingRenderPass::set_windows(const std::vector<RingWindowInstance>& windows) {
    windows_ = windows;
    pc_.window_count = static_cast<uint32_t>(windows_.size());
    dirty_ = true;
}

void RingRenderPass::record_into(FrameGraph& fg,
                                 const std::vector<ResourceHandle*>& /*window_textures*/,
                                 ResourceHandle* /*output*/) {
    fg.add_graphics_pass("RingOrbitalPass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd, 0.0f);
        });
}

void RingRenderPass::dispatch(VkCommandBuffer cmd, float time) {
    pc_.time = time;
    if (pipeline_ == VK_NULL_HANDLE || windows_.empty()) return;

    if (dirty_) {
        update_instance_buffer();
        dirty_ = false;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, pl_layout_, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(RingPushConstants), &pc_);

    vkCmdDraw(cmd, 6, static_cast<uint32_t>(windows_.size()), 0, 0);
}

void RingRenderPass::create_pipeline() {
    // Pipeline para Ring Switcher
}

void RingRenderPass::update_instance_buffer() {
    // Actualizar datos de instancias en VRAM
}

} // namespace compiz::core::passes
