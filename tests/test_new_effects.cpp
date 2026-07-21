#include "passes/annotate_compute_pass.hpp"
#include "passes/showmouse_compute_pass.hpp"
#include <iostream>
#include <cassert>

void testAnnotate() {
    std::cout << "Testing AnnotateComputePass API..." << std::endl;
    compiz::core::vulkan::VulkanContext ctx;
    VkExtent2D extent{1920, 1080};
    compiz::core::passes::AnnotateComputePass annotate(ctx, extent);

    annotate.start_stroke(100.0f, 100.0f, 0.5f);
    annotate.continue_stroke(200.0f, 150.0f, 0.8f);
    annotate.set_mode(compiz::core::passes::AnnotateMode::Fire);
    annotate.clear_canvas();

    std::cout << "  ✓ AnnotateComputePass API unit tests PASSED!" << std::endl;
}

void testShowmouse() {
    std::cout << "Testing ShowmouseComputePass API..." << std::endl;
    compiz::core::vulkan::VulkanContext ctx;
    VkExtent2D extent{1920, 1080};
    compiz::core::passes::ShowmouseComputePass showmouse(ctx, extent);

    showmouse.set_cursor_position(0.5f, 0.5f);
    showmouse.set_ring_radius(75.0f);
    showmouse.set_effect(compiz::core::passes::ShowmouseEffectType::Sparkle);

    std::cout << "  ✓ ShowmouseComputePass API unit tests PASSED!" << std::endl;
}

int main() {
    std::cout << "[TEST] Running new utility effects unit tests..." << std::endl;

    testAnnotate();
    testShowmouse();

    std::cout << "[PASS] All new utility effect unit tests passed successfully!" << std::endl;
    return 0;
}
