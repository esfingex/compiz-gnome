# Análisis Matemático y Modernización: Plugin Water (Superficie de Agua & Ripple)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/water/src/water.cpp`
- `compiz-0.9.14.2/plugins/water/src/shaders.h`
- `compiz-0.9.14.2/plugins/water/src/water.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `water` simula la propagación de ondas de agua en la superficie del escritorio usando la **Ecuación de Onda 2D Discretizada** por el Método de Diferencias Finitas (FDM), ejecutada mediante shaders de fragmentos con tres buffers intermitentes (Ping-Pong FBOs).

### A. Ecuación Diferencial Continua de Onda 2D
La física subyacente es la ecuación de onda en un medio bidimensional continuo:

$$\frac{\partial^2 u}{\partial t^2} = c^2 \left( \frac{\partial^2 u}{\partial x^2} + \frac{\partial^2 u}{\partial y^2} \right) - \gamma \frac{\partial u}{\partial t}$$

Donde:
- $u(x,y,t)$ es la altura de la superficie en la posición $(x,y)$ y tiempo $t$.
- $c$ es la velocidad de propagación de la onda.
- $\gamma$ es el coeficiente de amortiguamiento (viscosidad del fluido).

### B. Discretización Finitad en Malla 2D (Shader `update_water_vertices_fragment_shader`)
En el fragment shader de Compiz, la aceleración Laplacian $\nabla^2 u$ se calcula usando un stencil de 5 puntos (vecinos ortogonales):

$$\text{accel} = (u_{curr}(x, y-1) + u_{curr}(x, y+1) + u_{curr}(x-1, y) + u_{curr}(x+1, y)) - 4 \cdot u_{curr}(x, y)$$

La integración temporal de Verlet/Euler calcula la nueva altura $u_{next}(x,y)$ en el componente `alpha` (`v.w`):

$$u_{next}(x, y) = \text{accel} \cdot (dt \cdot K) + 2 \cdot u_{curr}(x, y) - u_{prev}(x, y)$$

$$u_{next}(x, y) \leftarrow u_{next}(x, y) \cdot \text{fade}$$

---

## 2. Refinamientos Físicos y Estabilidad Numérica AAA (Fixed Timestep & CFL Condition)

### A. Desacoplamiento Temporal: Acumulador Fixed Timestep ($\Delta t_{physics} = \frac{1}{120}\text{s}$)
En Compiz OpenGL, el escalado $dt \cdot K$ dependía del VSync. Si la tasa de refresco cambia (60Hz vs 144Hz) o sufre un drop a 30Hz, la simulación sufre **explosión numérica** o se congela.

En `compiz-gnome` C++20, se desacopla la física mediante un **Acumulador de Pasos Fijos**:

```cpp
class WaterSimulation {
    static constexpr float PHYSICS_DT = 1.0f / 120.0f; // 120Hz interno fijo
    float accumulator_ = 0.0f;

public:
    void step(float frameDt) {
        accumulator_ += frameDt;
        while (accumulator_ >= PHYSICS_DT) {
            dispatchCompute(PHYSICS_DT); // Push Constant dt = 1/120s
            accumulator_ -= PHYSICS_DT;
        }
    }
};
```

### B. Condición de Estabilidad CFL (Courant-Friedrichs-Lewy)
Para FDM explícito 2D con stencil de 5 puntos, el número de Courant debe cumplir:

$$C = c \cdot \frac{\Delta t}{\Delta x} \le \frac{1}{\sqrt{2}} \approx 0.707$$

El motor C++ valida y limita automáticamente la velocidad $c$ antes de enviarla como Push Constant:

$$c_{clamped} = \min\left(c_{user}, \; \frac{0.707}{\Delta t_{physics}}\right)$$

### C. Optimización de Formatos de Memoria VRAM
- **Heightmaps (Ping-Pong)**: `VK_FORMAT_R16_SFLOAT` (16-bit Float, ahorro de 2x ancho de banda VRAM respecto a R32).
- **Normal Map Output**: `VK_FORMAT_R16G16B16A16_SFLOAT` (Normales $XYZ$ empacadas en $RGB$ + Altura $u_{next}$ empacada en $Alpha$).

---

## 3. Kernel Vulkan Compute Shader Optimizado (`water_sim.comp`)

Con bloque local de $16 \times 16$ hilos y memoria compartida local $18 \times 18$ `f16` (648 Bytes en Local Data Store):

```glsl
#version 460 core
#extension GL_EXT_shader_explicit_arithmetic_types_f16 : require

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_prevHeight;
layout(set = 0, binding = 1) uniform image2D u_currHeight;
layout(set = 0, binding = 2) uniform image2D u_nextHeight;
layout(set = 0, binding = 3) uniform image2D u_normalMap;

layout(push_constant) uniform PushConstants {
    float dt;
    float fade;
    float waveSpeed;
    float deltaScale;
    uint  frameIndex;
} pc;

shared f16 tile[18][18]; // 648 Bytes LDS - Permite máximo occupancy por SM/Compute Unit

void main() {
    ivec2 globalId = ivec2(gl_GlobalInvocationID.xy);
    ivec2 localId  = ivec2(gl_LocalInvocationID.xy);
    ivec2 dim      = imageSize(u_currHeight);
    
    if (globalId.x >= dim.x || globalId.y >= dim.y) return;

    // 1. Carga cooperativa de halo 18x18 en Shared Memory
    uint linearId = gl_LocalInvocationIndex;
    for (uint i = linearId; i < 18 * 18; i += 256) {
        int tx = int(i % 18);
        int ty = int(i / 18);
        ivec2 loadPos = clamp(globalId - ivec2(1, 1) + ivec2(tx, ty), ivec2(0), dim - 1);
        tile[ty][tx] = f16(imageLoad(u_currHeight, loadPos).r);
    }
    
    barrier();

    int cx = localId.x + 1;
    int cy = localId.y + 1;

    float u_curr_c = f32(tile[cy][cx]);
    float u_prev_c = imageLoad(u_prevHeight, globalId).r;

    // 2. Laplaciano desde Shared Memory (0 lecturas VRAM adicionales)
    float laplacian = f32(tile[cy-1][cx]) + f32(tile[cy+1][cx]) +
                      f32(tile[cy][cx-1]) + f32(tile[cy][cx+1]) - 4.0 * u_curr_c;

    // 3. Integración de Verlet
    float c2_dt2 = pc.waveSpeed * pc.waveSpeed * pc.dt * pc.dt;
    float u_next = (laplacian * c2_dt2 + 2.0 * u_curr_c - u_prev_c) * pc.fade;

    imageStore(u_nextHeight, globalId, vec4(u_next, 0.0, 0.0, 0.0));

    // 4. Derivadas centrales para Normal Map (con empacado en RGBA16F)
    float dhdx = (f32(tile[cy][cx+1]) - f32(tile[cy][cx-1])) * 0.5 * pc.deltaScale;
    float dhdy = (f32(tile[cy-1][cx]) - f32(tile[cy+1][cx])) * 0.5 * pc.deltaScale; 

    vec3 N = normalize(vec3(-dhdx, -dhdy, 1.0));
    imageStore(u_normalMap, globalId, vec4(N * 0.5 + 0.5, u_next));
}
```

---

## 4. Fragment Shader del Compositor Clutter/Mutter (`water_effect.frag`)

Shader óptico ejecutado en el compositor con **Refracción de Snell + Aberración Cromática RGB + Reflexión de Fresnel**:

```glsl
#version 450 core
layout(binding = 0) uniform sampler2D u_desktopTexture;
layout(binding = 1) uniform sampler2D u_waterNormalMap;

in vec2 v_texCoord;
out vec4 fragColor;

uniform float u_refractionStrength;

void main() {
    vec4 waterData = texture(u_waterNormalMap, v_texCoord);
    vec3 N = waterData.rgb * 2.0 - 1.0;
    float height = waterData.a;

    // 1. Refracción de Snell (Aproximación de rayo)
    vec2 offset = (N.xy / max(N.z, 0.001)) * height * u_refractionStrength;
    vec2 texelSize = 1.0 / vec2(textureSize(u_desktopTexture, 0));

    // 2. Aberración Cromática Dispersiva RGB
    vec2 uv_r = v_texCoord + offset * 0.98 * texelSize;
    vec2 uv_g = v_texCoord + offset * 1.00 * texelSize;
    vec2 uv_b = v_texCoord + offset * 1.02 * texelSize;

    vec3 color;
    color.r = texture(u_desktopTexture, uv_r).r;
    color.g = texture(u_desktopTexture, uv_g).g;
    color.b = texture(u_desktopTexture, uv_b).b;

    // 3. Reflectividad de Fresnel (Schlick)
    float cosTheta = max(N.z, 0.0);
    float F0 = 0.02; // Agua
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);

    vec3 skyColor = vec3(0.1, 0.2, 0.35);
    color = mix(color, skyColor, fresnel * 0.4);

    // 4. Brillo Especular (Glint)
    float spec = pow(max(0.0, N.z), 200.0) * height * 2.0;
    color += vec3(spec);

    fragColor = vec4(color, 1.0);
}
```

---

## 5. Protocolo de Fences (Timeline Semaphores IPC Roundtrip)

```
C++ Vulkan Engine                       Kernel / SCM_RIGHTS                      Mutter / Clutter (Host)
      │                                         │                                           │
      ├──── Dispatch(WaterSim) ─────────────────┤                                           │
      ├──── Signal(TimelineSem @ N) ────────────┤                                           │
      ├──── Export Sync FD ────────────────────►│── Enviar {normal_fd, sync_fd, val=N} ─────►│
      │                                         │                                           ├─ Import Sync FD (Wait)
      │                                         │                                           ├─ Composite Water Pass
      │                                         │                                           ├─ Signal Release Sync FD
      │◄── Return Release Sync FD ──────────────┼◄── Enviar {release_fd, val=N} ─────────────┤
      ├──── Wait(Release Sync FD) ──────────────┤                                           │
      └──── Ready for Frame N+1 ────────────────┘                                           │
```

---

## 6. Implementación C++20: Clase `WaterComputePass` (Frame Graph Node)

Implementación concreta del nodo de simulación de agua desacoplado como parte del **Render Graph / Frame Graph Engine**:

```cpp
// WaterComputePass.hpp (C++20 Engine)
#pragma once
#include "vulkan/frame_graph.hpp"

class WaterComputePass : public EffectNode {
public:
    void setup(FrameGraph& fg, ResourceHandle inputDesktop, ResourceHandle outputNormalMap) override {
        // 1. Pass de simulación física Compute Shader (120Hz fixed dt)
        fg.addComputePass("WaterPhysicsCompute", [&](FrameGraphBuilder& builder) {
            builder.read(heightMapPrev_)
                   .write(heightMapCurr_)
                   .write(heightMapNext_)
                   .dispatch((width_ + 15) / 16, (height_ + 15) / 16, 1);
        });

        // 2. Pass de generación de mapa de normales y derivación de altura
        fg.addComputePass("WaterNormalMapGen", [&](FrameGraphBuilder& builder) {
            builder.read(heightMapNext_)
                   .write(outputNormalMap) // Registrado como FrameGraphResource exportable vía DMA-BUF
                   .setMemoryBarrier(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT);
        });
    }

    void execute(VkCommandBuffer cmd, FrameData& frameData) override {
        // Enlazar Descriptor Buffers VK_EXT_descriptor_buffer & Push Constants (dt, waveSpeed, fade)
        vkCmdBindDescriptorBuffersEXT(cmd, 1, &descriptorBufferBindingInfo_);
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(WaterPushConstants), &pushConstants_);
        vkCmdDispatch(cmd, (width_ + 15) / 16, (height_ + 15) / 16, 1);
    }

private:
    uint32_t width_ = 1920;
    uint32_t height_ = 1080;
    VkDescriptorBufferBindingInfoEXT descriptorBufferBindingInfo_{};
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    
    struct WaterPushConstants {
        float dt = 1.0f / 120.0f;
        float fade = 0.995f;
        float waveSpeed = 0.5f;
        float deltaScale = 1.0f / 1920.0f;
        uint32_t frameIndex = 0;
    } pushConstants_;
};
```
