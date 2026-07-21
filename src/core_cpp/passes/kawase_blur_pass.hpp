#pragma once

#include "../vulkan_context.hpp"
#include "../framegraph.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace compiz::core::passes {

// Numero maximo de niveles de mipmap en la cadena de downsample/upsample
static constexpr uint32_t KAWASE_MAX_LEVELS = 6;

// Push constants para el shader de downsample (compute)
struct KawaseDownPC {
    float offset;   // Desplazamiento del kernel (0.5 + nivel * 1.0)
};

// Push constants para el shader de upsample (fragment)
struct KawaseUpPC {
    float texel_w;  // 1.0 / width de la textura de entrada
    float texel_h;  // 1.0 / height de la textura de entrada
    float offset;   // Desplazamiento del kernel
    float opacity;  // Opacidad del blur [0,1]
};

// Blur Dual-Kawase de alta calidad:
// - N passes de downsample (compute, rgba16f)
// - N passes de upsample (fragment, rgba16f)
// - Radio variable: r = 2^N pixels equivalentes
// Complejidad: O(8*N) samples vs O(r^2) de un Gauss naive.
class KawaseBlurPass {
public:
    KawaseBlurPass(vulkan::VulkanContext& ctx, VkExtent2D extent, uint32_t levels = 3);
    ~KawaseBlurPass();

    bool init();
    void cleanup();

    // Ajusta radio de blur en runtime (0 = sin blur, 80 = blur maximo)
    void set_radius(float radius);

    // Registra los N passes down + N passes up en el FrameGraph
    void record_into(FrameGraph& fg,
                     ResourceHandle* input,
                     ResourceHandle* output);

    // Dispatch directo sin FrameGraph
    void dispatch(VkCommandBuffer cmd);

private:
    uint32_t compute_levels(float radius) const;
    bool create_pipelines();
    void create_intermediate_images();

    vulkan::VulkanContext& ctx_;
    VkExtent2D             extent_;
    uint32_t               max_levels_;
    float                  radius_{10.0f};
    uint32_t               active_levels_{3};

    // Pipelines
    VkDescriptorSetLayout  ds_layout_down_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_down_{VK_NULL_HANDLE};
    VkPipeline             pipeline_down_{VK_NULL_HANDLE};

    VkDescriptorSetLayout  ds_layout_up_{VK_NULL_HANDLE};
    VkPipelineLayout       pl_layout_up_{VK_NULL_HANDLE};
    VkPipeline             pipeline_up_{VK_NULL_HANDLE};

    // Imagenes intermedias (mip chain para downsample)
    struct MipLevel {
        VkImage        image{VK_NULL_HANDLE};
        VkImageView    view{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDescriptorSet ds{VK_NULL_HANDLE};
        VkExtent2D     extent{};
    };
    std::vector<MipLevel> mip_chain_;
    VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
};

} // namespace compiz::core::passes
