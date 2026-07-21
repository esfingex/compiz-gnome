#include "showmouse_compute_pass.hpp"

namespace compiz::core::passes {

ShowmouseComputePass::ShowmouseComputePass(vulkan::VulkanContext& ctx, VkExtent2D extent, uint32_t num_particles)
    : ctx_(ctx), extent_(extent), num_particles_(num_particles) {
    pc_.num_particles = num_particles_;
    pc_.ring_radius = 50.0f;
    pc_.effect_type = 0;
}

ShowmouseComputePass::~ShowmouseComputePass() {
    cleanup();
}

bool ShowmouseComputePass::init() {
    return true;
}

void ShowmouseComputePass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    if (compute_pipeline_) { vkDestroyPipeline(dev, compute_pipeline_, nullptr); compute_pipeline_ = VK_NULL_HANDLE; }
    if (pl_layout_)        { vkDestroyPipelineLayout(dev, pl_layout_, nullptr); pl_layout_ = VK_NULL_HANDLE; }
    if (ds_layout_)        { vkDestroyDescriptorSetLayout(dev, ds_layout_, nullptr); ds_layout_ = VK_NULL_HANDLE; }
    if (particle_buffer_)  { vkDestroyBuffer(dev, particle_buffer_, nullptr); particle_buffer_ = VK_NULL_HANDLE; }
    if (particle_memory_)  { vkFreeMemory(dev, particle_memory_, nullptr); particle_memory_ = VK_NULL_HANDLE; }
}

void ShowmouseComputePass::set_cursor_position(float x, float y) {
    pc_.cursor_pos[0] = x;
    pc_.cursor_pos[1] = y;
}

void ShowmouseComputePass::record_into(FrameGraph& fg, ResourceHandle* /*output_texture*/) {
    fg.add_compute_pass("ShowmouseParticlePass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd, 1.0f / 120.0f);
        });
}

void ShowmouseComputePass::dispatch(VkCommandBuffer cmd, float dt) {
    elapsed_time_ += dt;
    pc_.time = elapsed_time_;
    pc_.delta_time = dt;

    if (compute_pipeline_ == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_);
    vkCmdPushConstants(cmd, pl_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ShowmousePushConstants), &pc_);
    uint32_t groups = (num_particles_ + 63) / 64;
    vkCmdDispatch(cmd, groups, 1, 1);
}

} // namespace compiz::core::passes
