#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace compiz::core::passes {

// Datos de instancia por ventana en el anillo 3D (layout coincide con el SSBO del shader)
struct RingWindowInstance {
    float    position[2];  // x=angulo propio, y=offset vertical
    float    z_depth;
    float    scale;
    float    rotation;
    float    alpha;
    float    size[2];      // Tamano en NDC
    uint32_t window_id;
    uint32_t _pad;         // Alineacion a 16 bytes
};

// Push constants del vertex shader de orbita 3D
struct RingPushConstants {
    float    total_angle;    // Rotacion global animada del carrusel
    float    radius;         // Radio del anillo en NDC (0.3-0.8)
    float    tilt_angle;     // Inclinacion del anillo (0=plano, PI/6=30deg)
    float    scale_factor;   // Factor de perspectiva
    float    spacing;        // Espaciado angular entre ventanas (radianes)
    uint32_t window_count;
    float    time;
    float    _pad;
};

// Render Pass para el Ring Window Switcher 3D:
// - Dispone las ventanas en un anillo 3D inclinado.
// - Perspectiva simple por profundidad.
// - Animacion de rotacion global suave.
// - Ventana central mas grande y opaca que las laterales.
class RingRenderPass {
public:
    RingRenderPass(vulkan::VulkanContext& ctx, VkExtent2D extent);
    ~RingRenderPass();

    bool init();
    void cleanup();

    // Actualiza la lista de ventanas en el anillo
    void set_windows(const std::vector<RingWindowInstance>& windows);
    // Rota el carrusel animadamente
    void set_angle(float angle)         { pc_.total_angle = angle; }
    void set_radius(float radius)       { pc_.radius = radius; }
    void set_tilt(float tilt)           { pc_.tilt_angle = tilt; }
    void set_scale_factor(float sf)     { pc_.scale_factor = sf; }
    void set_spacing(float sp)          { pc_.spacing = sp; }

    // Registra el draw pass en el FrameGraph
    void record_into(FrameGraph& fg,
                     const std::vector<ResourceHandle*>& window_textures,
                     ResourceHandle* output);

    void dispatch(VkCommandBuffer cmd, float time);

private:
    void create_pipeline();
    void update_instance_buffer();

    vulkan::VulkanContext& ctx_;
    VkExtent2D             extent_;

    VkDescriptorSetLayout  ds_layout_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_{VK_NULL_HANDLE};
    VkPipeline             pipeline_{VK_NULL_HANDLE};
    VkDescriptorPool       descriptor_pool_{VK_NULL_HANDLE};

    VkBuffer        instance_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory  instance_memory_{VK_NULL_HANDLE};

    RingPushConstants                pc_{};
    std::vector<RingWindowInstance>  windows_;
    bool                             dirty_{false};
};

} // namespace compiz::core::passes
