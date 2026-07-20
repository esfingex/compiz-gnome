#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>

namespace compiz::core::vulkan {

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool init_headless();
    void cleanup();

    VkInstance instance() const noexcept { return instance_; }
    VkPhysicalDevice physical_device() const noexcept { return physical_device_; }
    VkDevice device() const noexcept { return device_; }
    VkQueue graphics_queue() const noexcept { return graphics_queue_; }
    uint32_t queue_family_index() const noexcept { return queue_family_index_; }

    // Crea un Timeline Semaphore exportable como sync_fd
    VkSemaphore create_timeline_semaphore(uint64_t initial_value = 0);
    int export_semaphore_fd(VkSemaphore semaphore);

    // Consulta la lista de DRM Format Modifiers soportados para un formato
    std::vector<uint64_t> get_supported_drm_modifiers(VkFormat format);

private:
    VkInstance instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};
    VkQueue graphics_queue_{VK_NULL_HANDLE};
    uint32_t queue_family_index_{0};
};

} // namespace compiz::core::vulkan
