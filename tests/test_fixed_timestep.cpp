#include "fixed_timestep.hpp"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

int main() {
    std::cout << "[TEST] Running FixedTimestep unit tests..." << std::endl;

    compiz::core::math::FixedTimestep timer(120.0); // 120Hz = 8.33ms

    assert(timer.dt_seconds() > 0.0083 && timer.dt_seconds() < 0.0084);

    float total_simulated_time = 0.0f;
    int steps = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(17));

    while (timer.check_and_step()) {
        total_simulated_time += static_cast<float>(timer.dt_seconds());
        steps++;
    }

    std::cout << "  ✓ Steps accumulated: " << steps << " (Expected ~2 at 60Hz presentation)" << std::endl;
    std::cout << "  ✓ Total simulated physical time: " << total_simulated_time * 1000.0f << "ms" << std::endl;

    assert(steps >= 1);

    std::cout << "[PASS] FixedTimestep unit tests passed successfully!" << std::endl;
    return 0;
}
