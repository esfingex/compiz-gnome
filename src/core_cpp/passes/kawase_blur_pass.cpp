#include "kawase_blur_pass.hpp"
#include <algorithm>
#include <cmath>

namespace compiz::core::passes {

KawaseBlurPass::KawaseBlurPass(vulkan::VulkanContext& ctx, VkExtent2D extent, uint32_t levels)
    : ctx_(ctx), extent_(extent), max_levels_(std::min(levels, KAWASE_MAX_LEVELS)) {
    active_levels_ = compute_levels(radius_);
}

KawaseBlurPass::~KawaseBlurPass() {
    cleanup();
}

bool KawaseBlurPass::init() {
    create_pipelines();
    create_intermediate_images();
    return true;
}

void KawaseBlurPass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    for (auto& mip : mip_chain_) {
        if (mip.view)   vkDestroyImageView(dev, mip.view, nullptr);
        if (mip.image)  vkDestroyImage(dev, mip.image, nullptr);
        if (mip.memory) vkFreeMemory(dev, mip.memory, nullptr);
    }
    mip_chain_.clear();

    if (pipeline_down_)   { vkDestroyPipeline(dev, pipeline_down_, nullptr); pipeline_down_ = VK_NULL_HANDLE; }
    if (pl_layout_down_)  { vkDestroyPipelineLayout(dev, pl_layout_down_, nullptr); pl_layout_down_ = VK_NULL_HANDLE; }
    if (ds_layout_down_)  { vkDestroyDescriptorSetLayout(dev, ds_layout_down_, nullptr); ds_layout_down_ = VK_NULL_HANDLE; }

    if (pipeline_up_)     { vkDestroyPipeline(dev, pipeline_up_, nullptr); pipeline_up_ = VK_NULL_HANDLE; }
    if (pl_layout_up_)    { vkDestroyPipelineLayout(dev, pl_layout_up_, nullptr); pl_layout_up_ = VK_NULL_HANDLE; }
    if (ds_layout_up_)    { vkDestroyDescriptorSetLayout(dev, ds_layout_up_, nullptr); ds_layout_up_ = VK_NULL_HANDLE; }

    if (descriptor_pool_) { vkDestroyDescriptorPool(dev, descriptor_pool_, nullptr); descriptor_pool_ = VK_NULL_HANDLE; }
}

void KawaseBlurPass::set_radius(float radius) {
    radius_ = radius;
    active_levels_ = compute_levels(radius);
}

void KawaseBlurPass::record_into(FrameGraph& fg,
                                ResourceHandle* /*input*/,
                                ResourceHandle* /*output*/) {
    fg.add_compute_pass("KawaseBlurDownPass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd);
        });
}

void KawaseBlurPass::dispatch(VkCommandBuffer cmd) {
    if (pipeline_down_ == VK_NULL_HANDLE || active_levels_ == 0) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_down_);

    for (uint32_t i = 0; i < active_levels_; ++i) {
        KawaseDownPC pc{ .offset = 0.5f + static_cast<float>(i) * 1.0f };
        vkCmdPushConstants(cmd, pl_layout_down_, VK_SHADER_STAGE_COMPUTE_BIT,
                           0, sizeof(KawaseDownPC), &pc);

        uint32_t w = std::max(1u, extent_.width >> (i + 1));
        uint32_t h = std::max(1u, extent_.height >> (i + 1));
        vkCmdDispatch(cmd, (w + 15) / 16, (h + 15) / 16, 1);
    }
}

uint32_t KawaseBlurPass::compute_levels(float radius) const {
    if (radius <= 0.0f) return 0;
    uint32_t l = static_cast<uint32_t>(std::floor(std::log2(std::max(1.0f, radius))));
    return std::min(l + 1, max_levels_);
}

bool KawaseBlurPass::create_pipelines() {
    return true;
}

void KawaseBlurPass::create_intermediate_images() {
    // Crear la cadena de mips para el downsample Dual Kawase
}

} // namespace compiz::core::passes
