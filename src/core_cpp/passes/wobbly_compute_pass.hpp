#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <mutex>
#include <vector>
#include <cstdint>

namespace compiz::core::passes {

// Parametros fisicos del sistema de resortes (layout compatible con push constants)
struct WobblyPushConstants {
    float    spring_k{0.80f};         // Constante elastica (N/m)
    float    friction{0.48f};         // Coeficiente de amortiguamiento viscoso
    float    mass{1.0f};              // Masa de cada vertice (kg)
    float    dt{1.0f / 120.0f};       // Paso de tiempo fijo (s)
    uint32_t grid_width{8};
    uint32_t grid_height{8};
};

// Impulso externo: arrastre de ventana o liberacion de boton
struct WobblyImpulse {
    float    x;          // Posicion normalizada [0,1] en el eje X de la ventana
    float    y;          // Posicion normalizada [0,1] en el eje Y de la ventana
    float    velocity_x; // Componente X de la velocidad del arrastre (pixels/s)
    float    velocity_y; // Componente Y de la velocidad del arrastre (pixels/s)
    float    strength;   // Intensidad del impulso [0,1]
};

// Nodo de efecto Wobbly Windows: simulacion de grilla de resortes en Compute +
// render de la malla deformada con bilineal interpolation en el Vertex shader.
class WobblyComputePass {
public:
    WobblyComputePass(vulkan::VulkanContext& ctx,
                      uint32_t grid_width  = 8,
                      uint32_t grid_height = 8);
    ~WobblyComputePass();

    // Configura parámetros físicos dinámicamente
    void set_physics_params(float spring_k, float friction, uint32_t grid_res);

    // Inicializa pipelines, buffers de resortes y posiciones de reposo
    bool init();
    void cleanup();

    // Aplica un impulso externo a la grilla (llamado desde IPC thread)
    void apply_impulse(const WobblyImpulse& impulse);

    // Aplica una fuerza de arrastre continua (move event de la ventana)
    void apply_grab_force(float dx, float dy, float grab_x, float grab_y);

    // Registra el compute pass en el FrameGraph
    void record_into(FrameGraph& fg,
                     ResourceHandle* input_texture,
                     ResourceHandle* output_texture);

    // Dispatch directo sin FrameGraph (para integracion legacy)
    void dispatch(VkCommandBuffer cmd, uint32_t frame_index);

    // Consultas de estado
    bool   is_settled() const;
    float  max_displacement() const;

private:
    void create_compute_pipeline();
    void create_grid_buffers();
    void initialize_rest_positions();

    vulkan::VulkanContext& ctx_;
    uint32_t grid_width_;
    uint32_t grid_height_;

    // Vulkan resources
    VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
    VkPipelineLayout      pipeline_layout_{VK_NULL_HANDLE};
    VkPipeline            compute_pipeline_{VK_NULL_HANDLE};
    VkDescriptorSet       descriptor_set_{VK_NULL_HANDLE};
    VkDescriptorPool      descriptor_pool_{VK_NULL_HANDLE};

    VkBuffer        grid_buffer_{VK_NULL_HANDLE};   // Posiciones + velocidades
    VkDeviceMemory  grid_memory_{VK_NULL_HANDLE};
    VkBuffer        rest_buffer_{VK_NULL_HANDLE};   // Posiciones de reposo
    VkDeviceMemory  rest_memory_{VK_NULL_HANDLE};

    WobblyPushConstants push_constants_{};

    // Thread-safety para impulsos desde hilo IPC
    mutable std::mutex impulse_mutex_;
    std::vector<WobblyImpulse> pending_impulses_;
    bool   needs_reset_{false};
};

} // namespace compiz::core::passes
