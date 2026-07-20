#pragma once

#include <cstdint>

namespace compiz::core::math {

struct WaveFDMConstants {
    static constexpr float PHYSICS_DT = 1.0f / 120.0f; // 120Hz fixed timestep (8.33ms)
    static constexpr float CFL_LIMIT = 0.70710678f;   // 1.0 / sqrt(2) para 2D grid stability
    static constexpr float DEFAULT_FADE = 0.995f;      // Factor de amortiguamiento de onda
    static constexpr float DEFAULT_WAVE_SPEED = 120.0f;// Velocidad de propagación de la onda (c)
    static constexpr uint32_t LDS_TILE_DIM = 18;      // 18x18 f16 shared memory tile (648 bytes)
};

} // namespace compiz::core::math
