#include "wobbly_compute_pass.hpp"
#include <iostream>
#include <cmath>
#include <cstring>

namespace compiz::core::passes {

WobblyComputePass::WobblyComputePass(vulkan::VulkanContext& ctx,
                                     uint32_t grid_width,
                                     uint32_t grid_height)
    : ctx_(ctx), grid_width_(grid_width), grid_height_(grid_height) {
    push_constants_.grid_width = grid_width_;
    push_constants_.grid_height = grid_height_;
}

WobblyComputePass::~WobblyComputePass() {
    cleanup();
}

void WobblyComputePass::set_physics_params(float spring_k, float friction, uint32_t grid_res) {
    push_constants_.spring_k = spring_k;
    push_constants_.friction = friction;
    if (grid_res >= 4 && grid_res <= 64) {
        grid_width_ = grid_res;
        grid_height_ = grid_res;
        push_constants_.grid_width = grid_width_;
        push_constants_.grid_height = grid_height_;
    }
}

bool WobblyComputePass::init() {
    create_compute_pipeline();
    create_grid_buffers();
    initialize_rest_positions();
    return true;
}

void WobblyComputePass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    if (compute_pipeline_)   { vkDestroyPipeline(dev, compute_pipeline_, nullptr); compute_pipeline_ = VK_NULL_HANDLE; }
    if (pipeline_layout_)    { vkDestroyPipelineLayout(dev, pipeline_layout_, nullptr); pipeline_layout_ = VK_NULL_HANDLE; }
    if (descriptor_set_layout_) { vkDestroyDescriptorSetLayout(dev, descriptor_set_layout_, nullptr); descriptor_set_layout_ = VK_NULL_HANDLE; }
    if (descriptor_pool_)    { vkDestroyDescriptorPool(dev, descriptor_pool_, nullptr); descriptor_pool_ = VK_NULL_HANDLE; }

    if (grid_buffer_) { vkDestroyBuffer(dev, grid_buffer_, nullptr); grid_buffer_ = VK_NULL_HANDLE; }
    if (grid_memory_) { vkFreeMemory(dev, grid_memory_, nullptr); grid_memory_ = VK_NULL_HANDLE; }
    if (rest_buffer_) { vkDestroyBuffer(dev, rest_buffer_, nullptr); rest_buffer_ = VK_NULL_HANDLE; }
    if (rest_memory_) { vkFreeMemory(dev, rest_memory_, nullptr); rest_memory_ = VK_NULL_HANDLE; }
}

void WobblyComputePass::apply_impulse(const WobblyImpulse& impulse) {
    std::lock_guard<std::mutex> lock(impulse_mutex_);
    pending_impulses_.push_back(impulse);
}

void WobblyComputePass::apply_grab_force(float dx, float dy, float grab_x, float grab_y) {
    apply_impulse(WobblyImpulse{
        .x = grab_x,
        .y = grab_y,
        .velocity_x = dx * 120.0f,
        .velocity_y = dy * 120.0f,
        .strength = 1.0f
    });
}

void WobblyComputePass::record_into(FrameGraph& fg,
                                    ResourceHandle* /*input_texture*/,
                                    ResourceHandle* /*output_texture*/) {
    fg.add_compute_pass("WobblySpringPass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd, 0);
        });
}

void WobblyComputePass::dispatch(VkCommandBuffer cmd, uint32_t /*frame_index*/) {
    if (compute_pipeline_ == VK_NULL_HANDLE) return;

    // Procesar impulsos pendientes del hilo IPC
    {
        std::lock_guard<std::mutex> lock(impulse_mutex_);
        pending_impulses_.clear();
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline_);

    if (descriptor_set_ != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_,
                                0, 1, &descriptor_set_, 0, nullptr);
    }

    vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(WobblyPushConstants), &push_constants_);

    uint32_t groups_x = (grid_width_ + 7) / 8;
    uint32_t groups_y = (grid_height_ + 7) / 8;

    vkCmdDispatch(cmd, groups_x, groups_y, 1);
}

bool WobblyComputePass::is_settled() const {
    return true;
}

float WobblyComputePass::max_displacement() const {
    return 0.0f;
}

void WobblyComputePass::create_compute_pipeline() {
    // Pipeline stub listo para recibir SPIR-V precompilado
}

void WobblyComputePass::create_grid_buffers() {
    // Allocate VkBuffer para grilla de resortes
}

void WobblyComputePass::initialize_rest_positions() {
    // Inicializar posiciones de reposo de la grilla
}

} // namespace compiz::core::passes
