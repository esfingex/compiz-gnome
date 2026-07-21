#include "burn_compute_pass.hpp"
#include <iostream>

namespace compiz::core::passes {

BurnComputePass::BurnComputePass(vulkan::VulkanContext& ctx, uint32_t max_particles)
    : ctx_(ctx), max_particles_(max_particles) {}

BurnComputePass::~BurnComputePass() {
    cleanup();
}

bool BurnComputePass::init() {
    return true;
}

void BurnComputePass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    std::lock_guard<std::mutex> lock(state_mutex_);
    for (auto& [id, state] : window_states_) {
        if (state.particle_buffer) vkDestroyBuffer(dev, state.particle_buffer, nullptr);
        if (state.particle_memory) vkFreeMemory(dev, state.particle_memory, nullptr);
        if (state.params_buffer)   vkDestroyBuffer(dev, state.params_buffer, nullptr);
        if (state.params_memory)   vkFreeMemory(dev, state.params_memory, nullptr);
    }
    window_states_.clear();

    if (pipeline_)         { vkDestroyPipeline(dev, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (pl_layout_)        { vkDestroyPipelineLayout(dev, pl_layout_, nullptr); pl_layout_ = VK_NULL_HANDLE; }
    if (ds_layout_)        { vkDestroyDescriptorSetLayout(dev, ds_layout_, nullptr); ds_layout_ = VK_NULL_HANDLE; }
    if (descriptor_pool_) { vkDestroyDescriptorPool(dev, descriptor_pool_, nullptr); descriptor_pool_ = VK_NULL_HANDLE; }
}

void BurnComputePass::start_burn(uint64_t window_id, float win_w, float win_h) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (window_states_.find(window_id) != window_states_.end()) return;
    create_window_state(window_id, win_w, win_h);
}

void BurnComputePass::stop_burn(uint64_t window_id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto it = window_states_.find(window_id);
    if (it != window_states_.end()) {
        VkDevice dev = ctx_.device();
        if (it->second.particle_buffer) vkDestroyBuffer(dev, it->second.particle_buffer, nullptr);
        if (it->second.particle_memory) vkFreeMemory(dev, it->second.particle_memory, nullptr);
        if (it->second.params_buffer)   vkDestroyBuffer(dev, it->second.params_buffer, nullptr);
        if (it->second.params_memory)   vkFreeMemory(dev, it->second.params_memory, nullptr);
        window_states_.erase(it);
    }
}

void BurnComputePass::set_progress(uint64_t window_id, float progress) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto it = window_states_.find(window_id);
    if (it != window_states_.end()) {
        it->second.progress = progress;
    }
}

void BurnComputePass::set_cursor(uint64_t /*window_id*/, float /*x*/, float /*y*/) {
    // Actualizar coordenadas del cursor para Firepaint manual
}

void BurnComputePass::record_into(FrameGraph& fg) {
    fg.add_compute_pass("BurnParticlesPass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd, 1.0f / 120.0f);
        });
}

void BurnComputePass::dispatch(VkCommandBuffer cmd, float dt) {
    elapsed_time_ += dt;
    if (pipeline_ == VK_NULL_HANDLE) return;

    std::lock_guard<std::mutex> lock(state_mutex_);
    for (auto& [id, state] : window_states_) {
        if (!state.active) continue;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);

        if (state.descriptor_set != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl_layout_,
                                    0, 1, &state.descriptor_set, 0, nullptr);
        }

        BurnPushConstants pc{
            .particleCount = state.particle_count,
            .time = elapsed_time_
        };

        vkCmdPushConstants(cmd, pl_layout_, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(BurnPushConstants), &pc);

        uint32_t groups = (state.particle_count + 255) / 256;
        vkCmdDispatch(cmd, groups, 1, 1);
    }
}

bool BurnComputePass::create_window_state(uint64_t window_id, float /*win_w*/, float /*win_h*/) {
    BurnWindowState state;
    state.particle_count = max_particles_;
    state.progress = 0.0f;
    state.active = true;

    window_states_[window_id] = state;
    return true;
}

} // namespace compiz::core::passes
