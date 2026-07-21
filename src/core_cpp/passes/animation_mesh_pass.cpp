#include "animation_mesh_pass.hpp"

namespace compiz::core::passes {

AnimationMeshPass::AnimationMeshPass(vulkan::VulkanContext& ctx, VkExtent2D extent)
    : ctx_(ctx), extent_(extent) {}

AnimationMeshPass::~AnimationMeshPass() {
    cleanup();
}

bool AnimationMeshPass::init() {
    create_tessellation_pipeline();
    return true;
}

void AnimationMeshPass::cleanup() {
    VkDevice dev = ctx_.device();
    if (dev == VK_NULL_HANDLE) return;

    std::lock_guard<std::mutex> lock(state_mutex_);
    window_states_.clear();

    if (tess_pipeline_) { vkDestroyPipeline(dev, tess_pipeline_, nullptr); tess_pipeline_ = VK_NULL_HANDLE; }
    if (pl_layout_)    { vkDestroyPipelineLayout(dev, pl_layout_, nullptr); pl_layout_ = VK_NULL_HANDLE; }
    if (ds_layout_)    { vkDestroyDescriptorSetLayout(dev, ds_layout_, nullptr); ds_layout_ = VK_NULL_HANDLE; }
}

void AnimationMeshPass::start_animation(uint64_t window_id,
                                        AnimationEffectType effect,
                                        bool opening,
                                        float duration_secs) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    window_states_[window_id] = AnimationWindowState{
        .effect = effect,
        .progress = opening ? 0.0f : 1.0f,
        .duration = duration_secs,
        .elapsed = 0.0f,
        .opening = opening,
        .active = true,
    };
}

bool AnimationMeshPass::update(float dt) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    bool any_active = false;

    for (auto it = window_states_.begin(); it != window_states_.end();) {
        auto& state = it->second;
        if (!state.active) {
            it = window_states_.erase(it);
            continue;
        }

        state.elapsed += dt;
        float p = state.elapsed / state.duration;
        if (p >= 1.0f) {
            state.progress = state.opening ? 1.0f : 0.0f;
            state.active = false;
            it = window_states_.erase(it);
        } else {
            state.progress = state.opening ? p : (1.0f - p);
            any_active = true;
            ++it;
        }
    }

    return any_active;
}

void AnimationMeshPass::record_into(FrameGraph& fg,
                                    const std::vector<ResourceHandle*>& /*window_textures*/,
                                    ResourceHandle* /*output*/) {
    fg.add_graphics_pass("AnimationSuiteTessPass",
        [this](VkCommandBuffer cmd) {
            this->dispatch(cmd);
        });
}

void AnimationMeshPass::dispatch(VkCommandBuffer cmd) {
    if (tess_pipeline_ == VK_NULL_HANDLE) return;

    std::lock_guard<std::mutex> lock(state_mutex_);
    for (const auto& [id, state] : window_states_) {
        if (!state.active) continue;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tess_pipeline_);

        AnimationPushConstants pc{
            .instance_index = static_cast<uint32_t>(id),
            .progress = state.progress,
            .tess_factor = 16.0f,
            .time = state.elapsed,
            .effect_type = static_cast<uint32_t>(state.effect),
            .extra_x = state.icon_x,
            .extra_y = state.icon_y,
            .extra_z = 0.5f,
            .extra_w = 0.5f,
        };

        vkCmdPushConstants(cmd, pl_layout_,
                           VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                           0, sizeof(AnimationPushConstants), &pc);

        vkCmdDraw(cmd, 4, 1, 0, 0);
    }
}

bool AnimationMeshPass::is_animating(uint64_t window_id) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto it = window_states_.find(window_id);
    return it != window_states_.end() && it->second.active;
}

float AnimationMeshPass::get_progress(uint64_t window_id) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto it = window_states_.find(window_id);
    if (it != window_states_.end()) return it->second.progress;
    return 1.0f;
}

void AnimationMeshPass::create_tessellation_pipeline() {
    // Pipeline para Animation Suite con Tessellation Control & Evaluation
}

} // namespace compiz::core::passes
