#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>

namespace compiz::core {

using ShaderReloadCallback = std::function<void(const std::string& name, const std::string& path)>;

// Sistema de Hot-Reloading de Shaders en tiempo real.
// Monitorea cambios en disco de los archivos GLSL/SPIR-V y notifica
// a las clases de efecto para recrear los pipelines Vulkan al vuelo.
class ShaderHotReload {
public:
    ShaderHotReload();
    ~ShaderHotReload();

    void watch_shader(const std::string& name, const std::string& filepath, ShaderReloadCallback callback);
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_.load(); }

    // Verifica cambios pendiente desde el hilo principal de renderizado
    void poll_changes();

private:
    struct WatchedShader {
        std::string name;
        std::string filepath;
        std::filesystem::file_time_type last_modified;
        ShaderReloadCallback callback;
    };

    void watch_loop();

    std::unordered_map<std::string, WatchedShader> watched_shaders_;
    std::vector<std::string>                       changed_shaders_;
    std::mutex                                     mutex_;
    std::thread                                    watch_thread_;
    std::atomic<bool>                              running_{true};
    std::atomic<bool>                              enabled_{true};
};

} // namespace compiz::core
