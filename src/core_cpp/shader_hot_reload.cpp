#include "shader_hot_reload.hpp"
#include <iostream>

namespace compiz::core {

ShaderHotReload::ShaderHotReload() {
    watch_thread_ = std::thread(&ShaderHotReload::watch_loop, this);
}

ShaderHotReload::~ShaderHotReload() {
    running_ = false;
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
}

void ShaderHotReload::watch_shader(const std::string& name,
                                   const std::string& filepath,
                                   ShaderReloadCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::filesystem::file_time_type mod_time{};
    if (std::filesystem::exists(filepath)) {
        mod_time = std::filesystem::last_write_time(filepath);
    }
    watched_shaders_[name] = WatchedShader{
        .name = name,
        .filepath = filepath,
        .last_modified = mod_time,
        .callback = callback,
    };
}

void ShaderHotReload::poll_changes() {
    std::vector<std::string> to_notify;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        to_notify.swap(changed_shaders_);
    }

    for (const auto& name : to_notify) {
        auto it = watched_shaders_.find(name);
        if (it != watched_shaders_.end() && it->second.callback) {
            std::cout << "🔄 Hot-reloading shader: " << name << " (" << it->second.filepath << ")" << std::endl;
            it->second.callback(name, it->second.filepath);
        }
    }
}

void ShaderHotReload::watch_loop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (!enabled_.load()) continue;

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [name, shader] : watched_shaders_) {
            if (!std::filesystem::exists(shader.filepath)) continue;
            auto current_time = std::filesystem::last_write_time(shader.filepath);
            if (current_time > shader.last_modified) {
                shader.last_modified = current_time;
                changed_shaders_.push_back(name);
            }
        }
    }
}

} // namespace compiz::core
