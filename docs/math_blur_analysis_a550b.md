# Análisis Matemático y Modernización: Plugin Blur (Desenfoque Gaussiano LDS & Dual Kawase)

## Fuentes de Referencia
- `wayfire/plugins/blur/gaussian.cpp`
- `wayfire/plugins/blur/kawase.cpp`
- `compiz-0.9.14.2/plugins/blur/src/blur.cpp`

---

## 1. Deconstrucción Matemática y Estrategia Dual AAA

El desenfoque de fondo en tiempo real a 4K/144Hz requiere seleccionar la estrategia de filtrado óptima en función del radio deseado:

| Radio de Blur ($r$) | Estrategia Vulkan | Complejidad | Justificación de Hardware |
| :--- | :--- | :--- | :--- |
| **Pequeño / Medio ($r < 15\text{px}$)** | **Gaussiano 2 pases con LDS Shared Memory** | $O(r)$ | Kernel de convolución en memoria compartida (8 KB LDS). |
| **Grande ($r \ge 15\text{px}$)** | **Dual Kawase (Down Compute + Up Graphics)** | $O(1)$ | Cadena de downsampling / upsampling con consumo constante de VRAM. |

---

## 2. Gaussiano Separable en Compute Shader (`blur_gaussian.comp`)

En Vulkan Compute Shader con `imageLoad`, no existe filtrado bilineal hardware "gratis" como en Fragment Shaders. Para maximizar el ancho de banda VRAM, se utiliza **Shared Memory (Local Data Store)** con halo de $r$ píxeles:

```glsl
#version 460
#extension GL_EXT_shader_explicit_arithmetic_types_f16 : require

layout(local_size_x = 32, local_size_y = 8, local_size_z = 1) in;

layout(set=0, binding=0) uniform image2D imgInput;  // RGBA16F
layout(set=0, binding=1) uniform image2D imgTemp;   // RGBA16F Intermediate
layout(set=0, binding=2) uniform image2D imgOutput; // RGBA16F Output

layout(push_constant) uniform GaussianPushConstants {
    ivec2 resolution;
    float invSigma2;
    int radius;
    int pass; // 0 = Horizontal (LDS), 1 = Vertical
} pc;

shared vec4 tile_h[64][8]; // 8 KB LDS

void main() {
    ivec2 global = ivec2(gl_GlobalInvocationID.xy);
    ivec2 local  = ivec2(gl_LocalInvocationID.xy);
    ivec2 dim    = imageSize(imgInput);

    if (global.x >= dim.x || global.y >= dim.y) return;

    if (pc.pass == 0) { // Pase Horizontal con LDS
        tile_h[local.x + pc.radius][local.y] = imageLoad(imgInput, ivec2(clamp(global.x, 0, dim.x-1), global.y));

        if (local.x < pc.radius)
            tile_h[local.x][local.y] = imageLoad(imgInput, ivec2(clamp(global.x - pc.radius, 0, dim.x-1), global.y));
        if (local.x >= 32 - pc.radius)
            tile_h[local.x + 2*pc.radius][local.y] = imageLoad(imgInput, ivec2(clamp(global.x + pc.radius, 0, dim.x-1), global.y));

        barrier();

        vec4 sum = vec4(0.0);
        float weightSum = 0.0;

        for (int i = -pc.radius; i <= pc.radius; ++i) {
            float x = float(i);
            float w = exp(-x * x * pc.invSigma2);
            sum += tile_h[local.x + pc.radius + i][local.y] * w;
            weightSum += w;
        }

        imageStore(imgTemp, global, sum / weightSum);
    } else { // Pase Vertical
        vec4 sum = vec4(0.0);
        float weightSum = 0.0;

        for (int i = -pc.radius; i <= pc.radius; ++i) {
            int y = clamp(global.y + i, 0, dim.y - 1);
            float x = float(i);
            float w = exp(-x * x * pc.invSigma2);
            sum += imageLoad(imgTemp, ivec2(global.x, y)) * w;
            weightSum += w;
        }
        imageStore(imgOutput, global, sum / weightSum);
    }
}
```

---

## 3. Dual Kawase Blur Híbrido (Compute Down + Graphics Up)

Para radios grandes ($r \ge 15\text{px}$), el número de iteraciones se calcula en CPU:

$$\text{iterations} = \max\left(1, \; \lfloor \log_2(\text{targetRadius}) \rfloor - 1\right)$$

### A. Down Pass Compute (`kawase_down.comp`)
```glsl
#version 460
layout(local_size_x = 16, local_size_y = 16) in;
layout(set=0, binding=0) uniform image2D src;
layout(set=0, binding=1) uniform image2D dst;

layout(push_constant) uniform PushConst { ivec2 srcSize; float offset; } pc;

void main() {
    ivec2 dstPos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dstSize = imageSize(dst);
    if (dstPos.x >= dstSize.x || dstPos.y >= dstSize.y) return;

    ivec2 c = ivec2((vec2(dstPos) * 2.0) + 0.5);
    vec4 sum = imageLoad(src, c) * 4.0;
    sum += imageLoad(src, c + ivec2(-1, 0) * pc.offset);
    sum += imageLoad(src, c + ivec2(1, 0) * pc.offset);
    sum += imageLoad(src, c + ivec2(0, -1) * pc.offset);
    sum += imageLoad(src, c + ivec2(0, 1) * pc.offset);

    imageStore(dst, dstPos, sum * 0.125);
}
```

### B. Up Pass Fragment Shader (`kawase_up.frag`)
El pase de subida usa el hardware TMU de texturizado para filtrado bilineal gratis (`VK_FILTER_LINEAR`):

```glsl
#version 450
layout(binding = 0) uniform sampler2D srcLow;
in vec2 vUV;
out vec4 fragColor;

uniform vec2 u_texel;
uniform float u_offset;

void main() {
    vec2 d = u_texel * u_offset;
    vec4 sum = texture(srcLow, vUV + vec2(-d.x * 2.0, 0.0)) * 1.0;
    sum += texture(srcLow, vUV + vec2(-d.x, d.y)) * 2.0;
    sum += texture(srcLow, vUV + vec2(0.0, d.y * 2.0)) * 1.0;
    sum += texture(srcLow, vUV + vec2(d.x, d.y)) * 2.0;
    sum += texture(srcLow, vUV + vec2(d.x * 2.0, 0.0)) * 1.0;
    sum += texture(srcLow, vUV + vec2(d.x, -d.y)) * 2.0;
    sum += texture(srcLow, vUV + vec2(0.0, -d.y * 2.0)) * 1.0;
    sum += texture(srcLow, vUV + vec2(-d.x, -d.y)) * 2.0;
    fragColor = sum / 12.0;
}
```

---

## 4. C++20 Frame Graph Node: `KawaseBlurPass` (con Memory Aliasing)

Las texturas transitorias `Down_i` y `Up_{N-i-1}` comparten la misma memoria `VkDeviceMemory` mediante VMA al tener dimensiones coincidentes en la cadena de reducciones:

```cpp
// KawaseBlurPass.hpp (C++20 Engine)
#pragma once
#include "vulkan/frame_graph.hpp"

class KawaseBlurPass : public EffectNode {
public:
    void setup(FrameGraph& fg, ResourceHandle input, ResourceHandle output, float radius) override {
        uint32_t iterations = std::max(1u, (uint32_t)std::floor(std::log2(radius)) - 1u);
        ResourceHandle current = input;

        // Down Passes (Compute)
        for (uint32_t i = 0; i < iterations; ++i) {
            ResourceHandle down = fg.createTransientImage("Kawase_Down_" + std::to_string(i),
                VK_FORMAT_R16G16B16A16_SFLOAT);
            fg.addComputePass("Kawase_Down_" + std::to_string(i), [=](FrameGraphBuilder& b) {
                b.read(current).write(down);
            });
            downHandles_.push_back(down);
            current = down;
        }

        // Up Passes (Graphics Fullscreen Triangle)
        for (int i = (int)iterations - 1; i >= 0; --i) {
            ResourceHandle up = (i == 0) ? output : fg.createTransientImage("Kawase_Up_" + std::to_string(i),
                VK_FORMAT_R16G16B16A16_SFLOAT); // Aliased with Down_i by VMA
            fg.addGraphicsPass("Kawase_Up_" + std::to_string(i), [=](FrameGraphBuilder& b) {
                b.read(current).write(up).renderFullscreenTriangle();
            });
            current = up;
        }
    }

    void execute(VkCommandBuffer cmd, FrameData& fd) override {}

private:
    std::vector<ResourceHandle> downHandles_;
};
```
