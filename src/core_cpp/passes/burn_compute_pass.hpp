#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include "../dmabuf_export.hpp"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <cstdint>

namespace compiz::core::passes {

// Layout del SSBO de particulas (debe coincidir con el struct del compute shader)
struct alignas(16) BurnParticle {
    float position[2];
    float velocity[2];
    float life;
    float maxLife;
    float color[3];
    float size;
    float _pad[2];        // std430: 48 bytes totales
};

// Parametros del efecto Burn (uniform buffer, actualizado desde C++ cada frame)
struct BurnUniformParams {
    float progress;       // [0,1] fraccion de la ventana quemada
    float dt;
    float emissionRate;
    float fadeSpeed;
    float windowSizeX;
    float windowSizeY;
    float cursorPosX;
    float cursorPosY;
    float burnWidth;
    float windSpeed;
    float _pad[2];
};

// Push constants del compute shader
struct BurnPushConstants {
    uint32_t particleCount;
    float    time;
};

// Estado de burn por ventana
struct BurnWindowState {
    VkBuffer        particle_buffer{VK_NULL_HANDLE};
    VkDeviceMemory  particle_memory{VK_NULL_HANDLE};
    VkBuffer        params_buffer{VK_NULL_HANDLE};
    VkDeviceMemory  params_memory{VK_NULL_HANDLE};
    VkDescriptorSet descriptor_set{VK_NULL_HANDLE};
    uint32_t        particle_count{0};
    float           progress{0.0f};
    bool            active{false};
};

// Efecto Burn / Firepaint: sistema de particulas GPU a 65k particulas.
// Simula la quema de ventanas con gravedad invertida, viento y gradiente
// de color por temperatura (negro → rojo → naranja → amarillo → blanco).
class BurnComputePass {
public:
    BurnComputePass(vulkan::VulkanContext& ctx, uint32_t max_particles = 65536);
    ~BurnComputePass();

    bool init();
    void cleanup();

    // Inicia el efecto de quemado en una ventana
    void start_burn(uint64_t window_id, float win_w, float win_h);
    // Detiene y elimina el estado de la ventana
    void stop_burn(uint64_t window_id);
    // Actualiza el progreso de quemado [0,1] (0=inicio, 1=toda quemada)
    void set_progress(uint64_t window_id, float progress);
    // Actualiza la posicion del cursor (para efecto Firepaint manual)
    void set_cursor(uint64_t window_id, float x, float y);

    // Registra el compute pass en el FrameGraph (1 dispatch por ventana activa)
    void record_into(FrameGraph& fg);

    // Dispatch directo
    void dispatch(VkCommandBuffer cmd, float dt);

private:
    bool create_window_state(uint64_t window_id, float win_w, float win_h);

    vulkan::VulkanContext& ctx_;
    uint32_t               max_particles_;

    VkDescriptorSetLayout  ds_layout_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_{VK_NULL_HANDLE};
    VkPipeline             pipeline_{VK_NULL_HANDLE};
    VkDescriptorPool       descriptor_pool_{VK_NULL_HANDLE};

    std::unordered_map<uint64_t, BurnWindowState> window_states_;
    mutable std::mutex state_mutex_;

    float elapsed_time_{0.0f};
};

} // namespace compiz::core::passes
