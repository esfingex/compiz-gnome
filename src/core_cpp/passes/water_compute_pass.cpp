#include "water_compute_pass.hpp"
#include <iostream>

namespace compiz::core::passes {

WaterComputePass::WaterComputePass(vulkan::VulkanContext& ctx, uint32_t width, uint32_t height)
    : ctx_(ctx), width_(width), height_(height) {}

WaterComputePass::~WaterComputePass() {
    cleanup();
}

bool WaterComputePass::init() {
    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(WaterPushConstants);

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant_range;

    if (vkCreatePipelineLayout(ctx_.device(), &layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        std::cerr << "[WaterComputePass] Error al crear VkPipelineLayout." << std::endl;
        return false;
    }

    std::cout << "[WaterComputePass] Inicializado nodo de simulación FDM (" << width_ << "x" << height_ << ") exitosamente." << std::endl;
    return true;
}

void WaterComputePass::inject_impact(float x, float y, float strength) {
    std::lock_guard<std::mutex> lock(impact_mutex_);
    push_constants_.impactX = x;
    push_constants_.impactY = y;
    push_constants_.impactStrength = strength;
}

void WaterComputePass::dispatch(VkCommandBuffer cmd, uint32_t frame_index) {
    std::lock_guard<std::mutex> lock(impact_mutex_);
    push_constants_.frameIndex = frame_index;

    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkCmdPushConstants(cmd, pipeline_layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(WaterPushConstants), &push_constants_);
    }

    // Workgroup dispatch (16x16 por tile)
    uint32_t group_x = (width_ + 15) / 16;
    uint32_t group_y = (height_ + 15) / 16;

    if (pipeline_sim_ != VK_NULL_HANDLE) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_sim_);
        vkCmdDispatch(cmd, group_x, group_y, 1);
    }

    // Reset impact después de inyectarlo en GPU
    push_constants_.impactX = -1.0f;
    push_constants_.impactY = -1.0f;
    push_constants_.impactStrength = 0.0f;
}

void WaterComputePass::cleanup() {
    if (pipeline_sim_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_.device(), pipeline_sim_, nullptr);
        pipeline_sim_ = VK_NULL_HANDLE;
    }
    if (pipeline_normals_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(ctx_.device(), pipeline_normals_, nullptr);
        pipeline_normals_ = VK_NULL_HANDLE;
    }
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(ctx_.device(), pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }
}

} // namespace compiz::core::passes
