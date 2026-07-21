#include "shift_render_pass.hpp"

namespace compiz::core::passes {

ShiftRenderPass::ShiftRenderPass(vulkan::VulkanContext& ctx, VkExtent2D extent)
    : ctx_(ctx), extent_(extent) {
    pc_.reflection_intensity = 0.6f;
    pc_.floor_y = -0.6f;
    pc_.fresnel_power = 3.0f;
    pc_.blur_amount = 0.5f;
    pc_.alpha = 1.0f;
}

ShiftRenderPass::~ShiftRenderPass() {
    cleanup();
}

bool ShiftRenderPass::init() {
    create_render_pipeline();
    create_reflection_pipeline();
    return true;
}

void ShiftRenderPass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    if (render_pipeline_)     { vkDestroyPipeline(dev, render_pipeline_, nullptr); render_pipeline_ = VK_NULL_HANDLE; }
    if (reflection_pipeline_) { vkDestroyPipeline(dev, reflection_pipeline_, nullptr); reflection_pipeline_ = VK_NULL_HANDLE; }
    if (pl_layout_)           { vkDestroyPipelineLayout(dev, pl_layout_, nullptr); pl_layout_ = VK_NULL_HANDLE; }
    if (ds_layout_)           { vkDestroyDescriptorSetLayout(dev, ds_layout_, nullptr); ds_layout_ = VK_NULL_HANDLE; }
    if (instance_buffer_)     { vkDestroyBuffer(dev, instance_buffer_, nullptr); instance_buffer_ = VK_NULL_HANDLE; }
    if (instance_memory_)     { vkFreeMemory(dev, instance_memory_, nullptr); instance_memory_ = VK_NULL_HANDLE; }
}

void ShiftRenderPass::update_windows(const std::vector<ShiftWindowTransform>& transforms) {
    window_transforms_ = transforms;
}

void ShiftRenderPass::record_into(FrameGraph& fg,
                                  const std::vector<ResourceHandle*>& /*window_textures*/,
                                  ResourceHandle* /*output*/) {
    fg.add_graphics_pass("ShiftCoverFlowPass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd);
        });
}

void ShiftRenderPass::dispatch(VkCommandBuffer cmd) {
    if (render_pipeline_ == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render_pipeline_);
    vkCmdPushConstants(cmd, pl_layout_, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(ShiftFragPC), &pc_);

    for (size_t i = 0; i < window_transforms_.size(); ++i) {
        vkCmdDraw(cmd, 6, 1, 0, static_cast<uint32_t>(i));
    }
}

void ShiftRenderPass::create_render_pipeline() {
    // Pipeline para Cover Flow rendering
}

void ShiftRenderPass::create_reflection_pipeline() {
    // Pipeline para reflexion planar espejada en el suelo
}

} // namespace compiz::core::passes
