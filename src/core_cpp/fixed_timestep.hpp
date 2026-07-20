#pragma once

#include <chrono>

namespace compiz::core::math {

class FixedTimestep {
public:
    explicit FixedTimestep(double target_hz = 120.0)
        : dt_(std::chrono::duration<double>(1.0 / target_hz)),
          accumulator_(std::chrono::duration<double>::zero()),
          last_time_(std::chrono::high_resolution_clock::now()) {}

    // Acumula el tiempo transcurrido desde la última llamada.
    // Retorna true si hay al menos un paso físico pendiente (120Hz = 8.33ms).
    bool check_and_step() {
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> frame_time = current_time - last_time_;

        // Clampear frame_time para evitar "spiral of death" si hay lag de composición (máx 250ms)
        if (frame_time.count() > 0.25) {
            frame_time = std::chrono::duration<double>(0.25);
        }

        last_time_ = current_time;
        accumulator_ += frame_time;

        if (accumulator_ >= dt_) {
            accumulator_ -= dt_;
            return true;
        }

        return false;
    }

    constexpr double dt_seconds() const noexcept { return dt_.count(); }

private:
    std::chrono::duration<double> dt_;
    std::chrono::duration<double> accumulator_;
    std::chrono::high_resolution_clock::time_point last_time_;
};

} // namespace compiz::core::math
