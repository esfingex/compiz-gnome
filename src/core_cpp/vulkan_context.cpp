#include "vulkan_context.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>

namespace compiz::core::vulkan {

VulkanContext::VulkanContext() = default;

VulkanContext::~VulkanContext() {
    cleanup();
}

bool VulkanContext::init_headless() {
    // 1. VkApplicationInfo
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "compiz-gnome-engine";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "CompizVulkanEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    // 2. Extensions
    std::vector<const char*> instance_extensions = {
        VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME
    };

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
    create_info.ppEnabledExtensionNames = instance_extensions.data();

    if (vkCreateInstance(&create_info, nullptr, &instance_) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Error al crear VkInstance 1.3" << std::endl;
        return false;
    }

    // 3. Select Physical Device
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);
    if (device_count == 0) {
        std::cerr << "[Vulkan] No se encontraron dispositivos con soporte Vulkan." << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(instance_, &device_count, devices.data());
    physical_device_ = devices[0]; // Selecciona la primera GPU disponible

    // 4. Find Graphics Queue Family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device_, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
            queue_family_index_ = i;
            break;
        }
    }

    // 5. Logical Device Extensions
    std::vector<const char*> device_extensions = {
        VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
        VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME
    };

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family_index_;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_features{};
    timeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timeline_features.timelineSemaphore = VK_TRUE;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = &timeline_features;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();

    if (vkCreateDevice(physical_device_, &device_create_info, nullptr, &device_) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Error al crear VkDevice lógico." << std::endl;
        return false;
    }

    vkGetDeviceQueue(device_, queue_family_index_, 0, &graphics_queue_);
    std::cout << "[Vulkan] Inicializado VkDevice 1.3 headless exitosamente." << std::endl;
    return true;
}

VkSemaphore VulkanContext::create_timeline_semaphore(uint64_t initial_value) {
    VkSemaphoreTypeCreateInfo timeline_type_info{};
    timeline_type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timeline_type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timeline_type_info.initialValue = initial_value;

    VkExportSemaphoreCreateInfo export_info{};
    export_info.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    export_info.pNext = &timeline_type_info;
    export_info.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = &export_info;

    VkSemaphore semaphore = VK_NULL_HANDLE;
    if (vkCreateSemaphore(device_, &semaphore_info, nullptr, &semaphore) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Error al crear VkSemaphore timeline exportable." << std::endl;
    }
    return semaphore;
}

int VulkanContext::export_semaphore_fd(VkSemaphore semaphore) {
    PFN_vkGetSemaphoreFdKHR pfnVkGetSemaphoreFdKHR =
        (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(device_, "vkGetSemaphoreFdKHR");

    if (!pfnVkGetSemaphoreFdKHR) {
        std::cerr << "[Vulkan] PFN_vkGetSemaphoreFdKHR no encontrado." << std::endl;
        return -1;
    }

    VkSemaphoreGetFdInfoKHR get_fd_info{};
    get_fd_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    get_fd_info.semaphore = semaphore;
    get_fd_info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

    int fd = -1;
    if (pfnVkGetSemaphoreFdKHR(device_, &get_fd_info, &fd) != VK_SUCCESS) {
        std::cerr << "[Vulkan] Error al exportar sync_fd del semáforo." << std::endl;
        return -1;
    }

    return fd;
}

std::vector<uint64_t> VulkanContext::get_supported_drm_modifiers(VkFormat format) {
    VkDrmFormatModifierPropertiesListEXT modifier_list{};
    modifier_list.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

    VkFormatProperties2 format_props2{};
    format_props2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
    format_props2.pNext = &modifier_list;

    vkGetPhysicalDeviceFormatProperties2(physical_device_, format, &format_props2);

    std::vector<VkDrmFormatModifierPropertiesEXT> props(modifier_list.drmFormatModifierCount);
    modifier_list.pDrmFormatModifierProperties = props.data();
    vkGetPhysicalDeviceFormatProperties2(physical_device_, format, &format_props2);

    std::vector<uint64_t> modifiers;
    for (const auto& prop : props) {
        modifiers.push_back(prop.drmFormatModifier);
    }
    return modifiers;
}

void VulkanContext::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

} // namespace compiz::core::vulkan
