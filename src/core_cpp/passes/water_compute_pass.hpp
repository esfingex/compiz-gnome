#pragma once

#include "../vulkan_context.hpp"
#include "../wave_fdm_constants.hpp"
#include <vulkan/vulkan.hpp>
#include <mutex>
#include <vector>

namespace compiz::core::passes {

struct WaterPushConstants {
    float dt{math::WaveFDMConstants::PHYSICS_DT};
    float fade{math::WaveFDMConstants::DEFAULT_FADE};
    float waveSpeed{math::WaveFDMConstants::DEFAULT_WAVE_SPEED};
    float deltaScale{1.0f / 1920.0f};
    uint32_t frameIndex{0};
    float impactX{-1.0f};
    float impactY{-1.0f};
    float impactStrength{0.0f};
};

class WaterComputePass {
public:
    explicit WaterComputePass(vulkan::VulkanContext& ctx, uint32_t width = 1920, uint32_t height = 1080);
    ~WaterComputePass();

    bool init();
    void cleanup();

    void inject_impact(float x, float y, float strength);
    void dispatch(VkCommandBuffer cmd, uint32_t frame_index);

private:
    vulkan::VulkanContext& ctx_;
    uint32_t width_;
    uint32_t height_;

    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline pipeline_sim_{VK_NULL_HANDLE};
    VkPipeline pipeline_normals_{VK_NULL_HANDLE};

    WaterPushConstants push_constants_{};
    std::mutex impact_mutex_;
};

} // namespace compiz::core::passes
