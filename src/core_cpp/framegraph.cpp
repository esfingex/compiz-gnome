#include "framegraph.hpp"
#include <stdexcept>

namespace compiz::core {

FrameGraph::FrameGraph(VkDevice device) : device_(device) {}

ResourceHandle* FrameGraph::create_transient_image(const std::string& name,
                                                    VkFormat /*format*/,
                                                    VkExtent2D /*extent*/) {
    auto [it, inserted] = resources_.emplace(name, ResourceHandle{next_id_++, name});
    if (!inserted) return &it->second;
    return &it->second;
}

void FrameGraph::add_compute_pass(const std::string& name,
                                   std::function<void(VkCommandBuffer)>&& fn,
                                   std::vector<ResourceHandle*> reads,
                                   std::vector<ResourceHandle*> writes) {
    passes_.push_back(FramePass{
        .name      = name,
        .record_fn = std::move(fn),
        .reads     = std::move(reads),
        .writes    = std::move(writes)
    });
}

void FrameGraph::add_graphics_pass(const std::string& name,
                                    std::function<void(VkCommandBuffer)>&& fn,
                                    std::vector<ResourceHandle*> reads,
                                    std::vector<ResourceHandle*> writes) {
    passes_.push_back(FramePass{
        .name      = name,
        .record_fn = std::move(fn),
        .reads     = std::move(reads),
        .writes    = std::move(writes)
    });
}

void FrameGraph::execute(VkCommandBuffer cmd) {
    for (auto& pass : passes_) {
        // Insertar barriers automáticos para todas las escrituras del pass
        for (auto* res : pass.writes) {
            if (res && res->image != VK_NULL_HANDLE) {
                insert_barrier(cmd, res,
                               VK_IMAGE_LAYOUT_GENERAL,
                               VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_MEMORY_READ_BIT,
                               VK_ACCESS_2_SHADER_WRITE_BIT);
            }
        }
        // Insertar barriers para lecturas
        for (auto* res : pass.reads) {
            if (res && res->image != VK_NULL_HANDLE) {
                insert_barrier(cmd, res,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                               VK_ACCESS_2_SHADER_WRITE_BIT,
                               VK_ACCESS_2_SHADER_READ_BIT);
            }
        }
        // Grabar el pass
        pass.record_fn(cmd);
    }
}

void FrameGraph::reset() {
    passes_.clear();
}

void FrameGraph::insert_barrier(VkCommandBuffer cmd,
                                 ResourceHandle* resource,
                                 VkImageLayout   new_layout,
                                 VkPipelineStageFlags2 src_stage,
                                 VkPipelineStageFlags2 dst_stage,
                                 VkAccessFlags2        src_access,
                                 VkAccessFlags2        dst_access) {
    VkImageMemoryBarrier2 barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext               = nullptr,
        .srcStageMask        = src_stage,
        .srcAccessMask       = src_access,
        .dstStageMask        = dst_stage,
        .dstAccessMask       = dst_access,
        .oldLayout           = resource->layout,
        .newLayout           = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = resource->image,
        .subresourceRange    = {
            .aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel    = 0,
            .levelCount      = 1,
            .baseArrayLayer  = 0,
            .layerCount      = 1,
        }
    };

    VkDependencyInfo dep{
        .sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext                   = nullptr,
        .dependencyFlags         = 0,
        .memoryBarrierCount      = 0,
        .pMemoryBarriers         = nullptr,
        .bufferMemoryBarrierCount= 0,
        .pBufferMemoryBarriers   = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers    = &barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dep);
    resource->layout = new_layout;
}

} // namespace compiz::core
