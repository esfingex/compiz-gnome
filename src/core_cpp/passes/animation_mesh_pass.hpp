#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace compiz::core::passes {

// Tipos de efecto de la Animation Suite
enum class AnimationEffectType : uint32_t {
    Fade      = 0,
    MagicLamp = 1,
    Curl      = 2,
    Explode   = 3,
    Slide     = 4,
};

// Push constants compartidos entre tesc y tese
struct AnimationPushConstants {
    uint32_t instance_index;
    float    progress;       // [0,1] progreso de la animacion
    float    tess_factor;    // Factor base de teselacion (4-16)
    float    time;
    uint32_t effect_type;    // AnimationEffectType
    float    extra_x;        // Magic Lamp: iconX, Curl: curlFront, Explode: seed
    float    extra_y;        // Magic Lamp: iconY
    float    extra_z;        // Magic Lamp: windowW
    float    extra_w;        // Magic Lamp: windowH
};

// Datos de una animacion activa por ventana
struct AnimationWindowState {
    AnimationEffectType effect{AnimationEffectType::Fade};
    float               progress{0.0f};    // [0,1]
    float               duration{0.4f};    // Duracion en segundos
    float               elapsed{0.0f};
    bool                opening{true};     // true=abrir, false=cerrar
    bool                active{false};
    float               icon_x{0.0f};
    float               icon_y{0.0f};
};

// Render Pass de la Animation Suite (Magic Lamp, Curl, Explode, Slide, Fade).
// Utiliza Tessellation (tesc + tese) para deformacion de malla adaptativa.
// Un unico par de shaders tesc/tese maneja todos los efectos via effectType.
class AnimationMeshPass {
public:
    AnimationMeshPass(vulkan::VulkanContext& ctx, VkExtent2D extent);
    ~AnimationMeshPass();

    bool init();
    void cleanup();

    // Inicia una animacion de apertura o cierre para una ventana
    void start_animation(uint64_t window_id,
                         AnimationEffectType effect,
                         bool opening,
                         float duration_secs = 0.35f);

    // Actualiza el progreso de todas las animaciones activas
    // Devuelve true si hay animaciones pendientes
    bool update(float dt);

    // Registra los draw calls en el FrameGraph (solo ventanas con animacion activa)
    void record_into(FrameGraph& fg,
                     const std::vector<ResourceHandle*>& window_textures,
                     ResourceHandle* output);

    void dispatch(VkCommandBuffer cmd);

    bool is_animating(uint64_t window_id) const;
    float get_progress(uint64_t window_id) const;

private:
    void create_tessellation_pipeline();

    vulkan::VulkanContext& ctx_;
    VkExtent2D             extent_;

    VkDescriptorSetLayout  ds_layout_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_{VK_NULL_HANDLE};
    VkPipeline             tess_pipeline_{VK_NULL_HANDLE};

    std::unordered_map<uint64_t, AnimationWindowState> window_states_;
    mutable std::mutex state_mutex_;
};

} // namespace compiz::core::passes
