#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <vulkan/vulkan.h>

namespace compiz::core {

// Handle de recurso transitorio (imagen Vulkan en la pila del FrameGraph)
struct ResourceHandle {
    uint32_t    id{0};
    std::string name;
    VkImage     image{VK_NULL_HANDLE};
    VkImageView image_view{VK_NULL_HANDLE};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
};

// Nodo de pass del FrameGraph
struct FramePass {
    std::string name;
    std::function<void(VkCommandBuffer)> record_fn;
    std::vector<ResourceHandle*> reads;
    std::vector<ResourceHandle*> writes;
};

// FrameGraph mínimo pero funcional para encadenar passes GPU
// Responsabilidades: orden de ejecución, barriers de imagen automáticos y
// álias de recursos transitorios (VMA) para cero overhead de VRAM.
class FrameGraph {
public:
    explicit FrameGraph(VkDevice device);
    ~FrameGraph() = default;

    // Registrar un recurso de imagen transitorio (gestionado por VMA)
    ResourceHandle* create_transient_image(const std::string& name,
                                            VkFormat format,
                                            VkExtent2D extent);

    // Registrar un compute pass en el grafo
    void add_compute_pass(const std::string& name,
                          std::function<void(VkCommandBuffer)>&& fn,
                          std::vector<ResourceHandle*> reads  = {},
                          std::vector<ResourceHandle*> writes = {});

    // Registrar un graphics pass en el grafo
    void add_graphics_pass(const std::string& name,
                           std::function<void(VkCommandBuffer)>&& fn,
                           std::vector<ResourceHandle*> reads  = {},
                           std::vector<ResourceHandle*> writes = {});

    // Ejecutar todos los passes registrados en orden topológico
    void execute(VkCommandBuffer cmd);

    // Limpiar todos los passes (llamar al final de frame)
    void reset();

private:
    // Inserta un VkImageMemoryBarrier2 automático entre cambios de layout
    void insert_barrier(VkCommandBuffer cmd,
                        ResourceHandle* resource,
                        VkImageLayout   new_layout,
                        VkPipelineStageFlags2 src_stage,
                        VkPipelineStageFlags2 dst_stage,
                        VkAccessFlags2        src_access,
                        VkAccessFlags2        dst_access);

    VkDevice                                          device_{VK_NULL_HANDLE};
    std::vector<FramePass>                            passes_;
    std::unordered_map<std::string, ResourceHandle>   resources_;
    uint32_t                                          next_id_{0};
};

} // namespace compiz::core
