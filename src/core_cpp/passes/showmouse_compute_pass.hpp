#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace compiz::core::passes {

enum class ShowmouseEffectType : uint32_t {
    Ring    = 0,
    Sparkle = 1,
    Trail   = 2,
    Ripple  = 3,
};

struct ShowmousePushConstants {
    float    cursor_pos[2];
    float    ring_radius;
    float    time;
    float    delta_time;
    uint32_t effect_type;
    uint32_t num_particles;
    uint32_t _pad;
};

// Render Pass para Showmouse (Partículas alrededor del cursor).
// Emite anillos brillantes, chispas o estelas siguiendo el puntero.
class ShowmouseComputePass {
public:
    ShowmouseComputePass(vulkan::VulkanContext& ctx, VkExtent2D extent, uint32_t num_particles = 512);
    ~ShowmouseComputePass();

    bool init();
    void cleanup();

    void set_cursor_position(float x, float y);
    void set_ring_radius(float radius) { pc_.ring_radius = radius; }
    void set_effect(ShowmouseEffectType effect) { pc_.effect_type = static_cast<uint32_t>(effect); }

    void record_into(FrameGraph& fg, ResourceHandle* output_texture);
    void dispatch(VkCommandBuffer cmd, float dt);

private:
    vulkan::VulkanContext& ctx_;
    VkExtent2D             extent_;
    uint32_t               num_particles_;

    VkDescriptorSetLayout  ds_layout_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_{VK_NULL_HANDLE};
    VkPipeline             compute_pipeline_{VK_NULL_HANDLE};

    VkBuffer               particle_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory         particle_memory_{VK_NULL_HANDLE};

    ShowmousePushConstants pc_{};
    float                  elapsed_time_{0.0f};
};

} // namespace compiz::core::passes
