#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace compiz::core::passes {

// Transform de una ventana en el carrusel Shift/Cover Flow
struct ShiftWindowTransform {
    uint64_t window_id;
    float    angle;         // Angulo en el carrusel [0, 2PI]
    float    alpha;         // Opacidad [0,1] (la central = 1.0)
    float    scale;         // Escala (la central = 1.0)
    float    z_offset;      // Profundidad en NDC
    float    ndc_x;         // Posicion horizontal final en NDC
    float    ndc_y;
};

// Push constants para el fragment shader de reflexion planar
struct ShiftFragPC {
    float reflection_intensity;  // [0,1]
    float floor_y;               // Y del plano de suelo (NDC)
    float fresnel_power;
    float blur_amount;           // [0,1]
    float alpha;
    float scale;
    float rotation;
    float offset_x;
    float offset_y;
    float _pad[3];               // 48 bytes totales alineados
};

// Render Pass para el efecto Shift Switcher / Cover Flow:
// - Renderiza las ventanas en carrusel con reflexion planar en el suelo.
// - Blur glossy en el reflejo proporcional a la distancia al suelo.
// - Gradiente lateral de brillo para ventanas no seleccionadas.
class ShiftRenderPass {
public:
    ShiftRenderPass(vulkan::VulkanContext& ctx, VkExtent2D extent);
    ~ShiftRenderPass();

    bool init();
    void cleanup();

    // Actualiza la lista de ventanas con sus transforms
    void update_windows(const std::vector<ShiftWindowTransform>& transforms);
    // Ajusta la Y del plano de reflexion en NDC
    void set_reflection_plane(float y) { reflection_plane_y_ = y; }
    // Intensidad del reflejo
    void set_reflection_intensity(float i) { pc_.reflection_intensity = i; }

    // Registra los passes en el FrameGraph:
    // 1. Kawase Blur (reflexion glossy)
    // 2. Render de ventanas + reflexion
    void record_into(FrameGraph& fg,
                     const std::vector<ResourceHandle*>& window_textures,
                     ResourceHandle* output);

    void dispatch(VkCommandBuffer cmd);

private:
    void create_render_pipeline();
    void create_reflection_pipeline();

    vulkan::VulkanContext& ctx_;
    VkExtent2D             extent_;
    float                  reflection_plane_y_{-0.6f};

    VkDescriptorSetLayout  ds_layout_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_{VK_NULL_HANDLE};
    VkPipeline             render_pipeline_{VK_NULL_HANDLE};
    VkPipeline             reflection_pipeline_{VK_NULL_HANDLE};

    VkBuffer        instance_buffer_{VK_NULL_HANDLE};
    VkDeviceMemory  instance_memory_{VK_NULL_HANDLE};

    ShiftFragPC                      pc_{};
    std::vector<ShiftWindowTransform> window_transforms_;
};

} // namespace compiz::core::passes
