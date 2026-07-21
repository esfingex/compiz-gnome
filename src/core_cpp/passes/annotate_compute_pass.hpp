#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include <cstdint>

namespace compiz::core::passes {

enum class AnnotateMode : uint32_t {
    Fire      = 0,
    Ink       = 1,
    Highlight = 2,
    Eraser    = 3,
};

struct StrokePoint {
    float x;
    float y;
    float pressure;
    float timestamp;
};

// Render Pass para Annotate & Paintfire (Dibujo de fuego/tinta sobre pantalla).
// Permite trazos dinámicos en tiempo real impulsados por Compute Shader.
class AnnotateComputePass {
public:
    AnnotateComputePass(vulkan::VulkanContext& ctx, VkExtent2D extent, uint32_t max_particles = 50000);
    ~AnnotateComputePass();

    bool init();
    void cleanup();

    void start_stroke(float x, float y, float pressure = 1.0f);
    void continue_stroke(float x, float y, float pressure = 1.0f);
    void end_stroke();
    void clear_canvas();

    void set_mode(AnnotateMode mode) { current_mode_ = mode; }
    void set_color(float r, float g, float b, float a = 1.0f) {
        color_[0] = r; color_[1] = g; color_[2] = b; color_[3] = a;
    }

    void record_into(FrameGraph& fg, ResourceHandle* output_texture);
    void dispatch(VkCommandBuffer cmd);

private:
    vulkan::VulkanContext& ctx_;
    VkExtent2D             extent_;
    uint32_t               max_particles_;
    AnnotateMode           current_mode_{AnnotateMode::Ink};
    float                  color_[4]{1.0f, 0.2f, 0.1f, 1.0f};

    VkDescriptorSetLayout  ds_layout_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_{VK_NULL_HANDLE};
    VkPipeline             compute_pipeline_{VK_NULL_HANDLE};

    VkBuffer               particle_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory         particle_memory_{VK_NULL_HANDLE};

    std::vector<StrokePoint> stroke_history_;
    mutable std::mutex       stroke_mutex_;
    bool                     is_drawing_{false};
};

} // namespace compiz::core::passes
