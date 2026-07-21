// Struct de exportación DMA-BUF limpio. Centraliza todos los FDs y metadata
// que el motor C++ transfiere a GJS por IPC/SCM_RIGHTS.
#pragma once

#include <cstdint>

namespace compiz::core {

// Exportación de una imagen Vulkan como DMA-BUF listo para EGL
struct DmaBufExport {
    int      fd{-1};          // DMA-BUF memory FD (VkExternalMemoryHandleType::eDmaBufEXT)
    int      sync_fd{-1};     // Timeline Semaphore FD (señaliza cuando GPU terminó de escribir)
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};       // row pitch en bytes
    uint32_t drm_format{0};   // DRM_FORMAT_ARGB8888 / XRGB8888, etc.
    uint64_t modifier{0};     // DRM_FORMAT_MOD_LINEAR o modificador específico de GPU
    uint64_t timeline_value{0}; // Valor del semáforo al que sincronizar antes de leer
};

} // namespace compiz::core
