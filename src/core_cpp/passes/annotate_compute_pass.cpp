#include "annotate_compute_pass.hpp"

namespace compiz::core::passes {

AnnotateComputePass::AnnotateComputePass(vulkan::VulkanContext& ctx, VkExtent2D extent, uint32_t max_particles)
    : ctx_(ctx), extent_(extent), max_particles_(max_particles) {}

AnnotateComputePass::~AnnotateComputePass() {
    cleanup();
}

bool AnnotateComputePass::init() {
    return true;
}

void AnnotateComputePass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    if (compute_pipeline_) { vkDestroyPipeline(dev, compute_pipeline_, nullptr); compute_pipeline_ = VK_NULL_HANDLE; }
    if (pl_layout_)        { vkDestroyPipelineLayout(dev, pl_layout_, nullptr); pl_layout_ = VK_NULL_HANDLE; }
    if (ds_layout_)        { vkDestroyDescriptorSetLayout(dev, ds_layout_, nullptr); ds_layout_ = VK_NULL_HANDLE; }
    if (particle_buffer_)  { vkDestroyBuffer(dev, particle_buffer_, nullptr); particle_buffer_ = VK_NULL_HANDLE; }
    if (particle_memory_)  { vkFreeMemory(dev, particle_memory_, nullptr); particle_memory_ = VK_NULL_HANDLE; }
}

void AnnotateComputePass::start_stroke(float x, float y, float pressure) {
    std::lock_guard<std::mutex> lock(stroke_mutex_);
    is_drawing_ = true;
    stroke_history_.push_back({x, y, pressure, 0.0f});
}

void AnnotateComputePass::continue_stroke(float x, float y, float pressure) {
    std::lock_guard<std::mutex> lock(stroke_mutex_);
    if (is_drawing_) {
        stroke_history_.push_back({x, y, pressure, 0.0f});
    }
}

void AnnotateComputePass::end_stroke() {
    std::lock_guard<std::mutex> lock(stroke_mutex_);
    is_drawing_ = false;
}

void AnnotateComputePass::clear_canvas() {
    std::lock_guard<std::mutex> lock(stroke_mutex_);
    stroke_history_.clear();
}

void AnnotateComputePass::record_into(FrameGraph& fg, ResourceHandle* /*output_texture*/) {
    fg.add_compute_pass("AnnotateStrokePass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd);
        });
}

void AnnotateComputePass::dispatch(VkCommandBuffer cmd) {
    if (compute_pipeline_ == VK_NULL_HANDLE) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_);
    uint32_t groups_x = (extent_.width + 15) / 16;
    uint32_t groups_y = (extent_.height + 15) / 16;
    vkCmdDispatch(cmd, groups_x, groups_y, 1);
}

} // namespace compiz::core::passes
