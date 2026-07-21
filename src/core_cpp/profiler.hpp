#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <sstream>

namespace compiz::core {

// Sistema de Perfilado en Tiempo Real (CPU, GPU time, VRAM, FPS).
class RealTimeProfiler {
public:
    static RealTimeProfiler& get() {
        static RealTimeProfiler instance;
        return instance;
    }

    void begin_frame() {
        frame_start_ = std::chrono::steady_clock::now();
    }

    void end_frame() {
        auto frame_end = std::chrono::steady_clock::now();
        double frame_time_us = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start_).count()
        );
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_["frame_time_ms"] = frame_time_us / 1000.0;
        metrics_["fps"] = frame_time_us > 0.0 ? (1000000.0 / frame_time_us) : 120.0;
    }

    void record_gpu_time(const std::string& pass_name, double time_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_["gpu_" + pass_name + "_ms"] = time_ms;
    }

    void record_gpu_memory(uint64_t used_bytes, uint64_t total_bytes) {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_["gpu_memory_used_mb"] = static_cast<double>(used_bytes) / (1024.0 * 1024.0);
        metrics_["gpu_memory_total_mb"] = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
    }

    std::string export_json() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& [key, value] : metrics_) {
            if (!first) ss << ",";
            ss << "\"" << key << "\":" << value;
            first = false;
        }
        ss << "}";
        return ss.str();
    }

private:
    RealTimeProfiler() = default;
    ~RealTimeProfiler() = default;

    std::chrono::steady_clock::time_point frame_start_;
    std::unordered_map<std::string, double> metrics_;
    mutable std::mutex mutex_;
};

} // namespace compiz::core
