# Análisis Matemático y Modernización: Suite Completa de Animaciones (Magic Lamp, Curl, Explode, etc.)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/animation/src/magiclamp.cpp`
- `compiz-0.9.14.2/plugins/animation/src/curvedfold.cpp`
- `compiz-0.9.14.2/plugins/animation/src/dodge.cpp`
- `compiz-0.9.14.2/plugins/animation/src/glide.cpp`

---

## 1. Deconstrucción Matemática y Refinamientos AAA

El plugin `animation` deforma la malla 2D/3D de los vértices de la ventana durante las transiciones de apertura (`map`), cierre (`unmap`), minimizado y restaurado.

---

### A. Magic Lamp (Efecto Lámpara Mágica / Genie)

#### 1. Curva Sigmoide con $k$ Adaptativo
Para estirar la ventana hacia la posición del icono en el dock $(x_{icon}, y_{icon})$, la pendiente de la sigmoide $k$ se calcula dinámicamente según la duración y la distancia al icono:

$$k = \text{remap}(\text{duration}, 100\text{ms}\to 1000\text{ms}, 20.0 \to 2.0) \cdot \text{distanceFactor}$$

$$S(f_x) = \frac{1}{1 + e^{-k (f_x - 0.5)}}, \quad f_y = \frac{S(f_x) - S(0)}{S(1) - S(0)}$$

$$x_{target} = f_y \cdot (x_{orig} - x_{icon}) + x_{icon}$$

#### 2. Modulación de Ondas en Magic Lamp Wavy
$$\Delta x = \sum_{w=1}^{N_{waves}} \frac{A_w \cdot s_x}{2} \left[ \cos\left( \pi \cdot \frac{f_x - pos_w}{halfWidth_w} \right) + 1 \right]$$

---

### B. Curl / Curved Fold (Doblado de Página con Continuidad $C^1$)

Para evitar pliegues bruscos, los puntos de control $\mathbf{P}_1$ y $\mathbf{P}_2$ de la curva de Bézier Cúbica 3D se alinean con la tangente del plano XY:

$$\mathbf{P}_0 = (x, y_{bottom}, 0), \quad \mathbf{P}_3 = (x, y_{top}, 0)$$
$$\mathbf{P}_1 = \mathbf{P}_0 + \frac{1}{3}(0, H, 0) + (0, 0, Z_{max} \cdot \text{progress})$$
$$\mathbf{P}_2 = \mathbf{P}_3 - \frac{1}{3}(0, H, 0) + (0, 0, Z_{max} \cdot \text{progress})$$

$$\mathbf{B}(t) = (1-t)^3 \mathbf{P}_0 + 3(1-t)^2 t \mathbf{P}_1 + 3(1-t) t^2 \mathbf{P}_2 + t^3 \mathbf{P}_3, \quad t \in [0, 1]$$

---

### C. Explode (Integración de Verlet en Mesh Shader)

En lugar de Euler (que acumula inestabilidad si $dt$ varía), la física de fragmentos del triángulo utiliza **Integración de Verlet + Quaternion Slerp**:

$$\mathbf{P}_{next} = 2 \mathbf{P}_{curr} - \mathbf{P}_{prev} + \mathbf{a} \cdot \Delta t^2$$
$$\mathbf{Q}_{next} = \text{slerp}(\mathbf{Q}_{prev}, \mathbf{Q}_{curr}, \Delta t)$$

---

### D. Roll Up (Enrollado Cilíndrico con Protección $R_{min}$)

Para evitar la división por cero y valores $NaN$ cuando $R(t) \to 0$ al final del enrollado:

$$R = \max\left(R_{start} \cdot (1 - \text{progress}), \; 0.001 \cdot H_{window}\right)$$
$$y_{cyl} = y_{bottom} - R \cdot \sin\left(\frac{y_{orig} - y_{bottom}}{R}\right)$$
$$z_{cyl} = R \cdot \left(1 - \cos\left(\frac{y_{orig} - y_{bottom}}{R}\right)\right)$$

---

### E. Dodge (Campo de Fuerzas GPU $O(N^2)$)

El cálculo del campo de repulsión $\vec{F} = \sum \frac{k_{dodge}}{\|\Delta\mathbf{P}\|^2} \hat{\mathbf{r}}$ entre $N$ ventanas vecinas se delega a un **Compute Shader `DodgeForceCS`** con 1 indirect dispatch, evitando cuellos de botella en GJS.

---

## 2. Decisión Arquitectónica Híbrida: Mesh Shaders vs. Tessellation Shaders

| Característica | Tessellation Shaders (`VK_KHR_tessellation_shader`) | Mesh Shaders (`VK_EXT_mesh_shader`) |
| :--- | :--- | :--- |
| **Rol en el Motor** | **Fallback Runtime & Deformación Continua** | **Backend Principal de Alto Rendimiento** |
| **Efectos** | Curl, Roll Up, Magic Lamp Base, Wobbly | Explode, Burn, Firepaint, Magic Lamp Wavy |
| **Soporte HW** | Universal (Vulkan 1.0 Core + CI Software) | Vulkan 1.3+ (NVIDIA Ampere+, AMD RDNA2+, Intel Xe/ARC) |

---

## 3. Shaders GLSL de la Suite de Animaciones

### A. Tessellation Control Shader (`animation_tess.tesc`)
```glsl
#version 460
layout(vertices = 4) out;
layout(std430, set=0, binding=0) buffer InstanceData { AnimationInstanceData data[]; };
layout(push_constant) uniform PushConst { uint instanceIndex; float tessFactor; } pc;

void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    if (gl_InvocationID == 0) {
        float edgeTess = clamp(pc.tessFactor * (1.0 + data[pc.instanceIndex].params.y * 5.0), 2.0, 64.0);
        gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = gl_TessLevelOuter[3] = edgeTess;
        gl_TessLevelInner[0] = gl_TessLevelInner[1] = edgeTess;
    }
}
```

### B. Tessellation Evaluation Shader (`animation_tess.tese`)
```glsl
#version 460
layout(quads, equal_spacing, ccw) in;
layout(std430, set=0, binding=0) buffer InstanceData { AnimationInstanceData data[]; };
layout(push_constant) uniform PushConst { uint instanceIndex; float progress; } pc;

in vec2 vUV_in[];
out vec2 vUV;

vec3 magicLampDeform(vec2 uv, float prog, vec2 iconPos) {
    float k = clamp(10.0 * (1.0 - prog), 2.0, 20.0);
    float s0 = 1.0 / (1.0 + exp(k * 0.5));
    float s1 = 1.0 / (1.0 + exp(-k * 0.5));
    float sx = 1.0 / (1.0 + exp(-k * (uv.x - 0.5)));
    float fy = (sx - s0) / (s1 - s0);
    float x = mix(uv.x, iconPos.x, fy);
    return vec3(x * 2.0 - 1.0, uv.y * 2.0 - 1.0, 0.0);
}

void main() {
    vec2 uv = gl_TessCoord.xy;
    vUV = uv;
    AnimationInstanceData d = data[pc.instanceIndex];
    vec3 pos = vec3(uv * 2.0 - 1.0, 0.0);

    if (d.effectType == 1) pos = magicLampDeform(uv, pc.progress, d.extra1.xy);
    gl_Position = d.modelMatrix * vec4(pos, 1.0);
}
```

---

## 4. Implementación C++20: Clase `AnimationMeshPass` (Frame Graph Node)

```cpp
// AnimationMeshPass.hpp (C++20 Engine)
#pragma once
#include "vulkan/frame_graph.hpp"

class AnimationMeshPass : public EffectNode {
public:
    void setup(FrameGraph& fg) override {
        fg.usePersistentBuffer("Anim_Explode_Fragments", explodeFragmentsBuffer_);
        fg.usePersistentBuffer("Anim_Dodge_Offsets", dodgeOffsetsBuffer_);

        if (device_.hasMeshShader()) {
            fg.addMeshPass("Animation_Mesh_Explode", [&](FrameGraphBuilder& b) {
                b.read("Anim_Explode_Fragments").read("Anim_Instance_Data");
            });
        } else {
            fg.addGraphicsPass("Animation_Tess_Fallback", [&](FrameGraphBuilder& b) {
                b.read("Anim_Instance_Data")
                 .setTopology(VK_PRIMITIVE_TOPOLOGY_PATCH_LIST);
            });
        }
    }

    void execute(VkCommandBuffer cmd, FrameData& fd) override {
        if (useMesh_) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taskMeshPipeline_);
            vkCmdDrawMeshTasksIndirectEXT(cmd, indirectBuffer_, 0, 1, sizeof(VkDrawMeshTasksIndirectCommandEXT));
        } else {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tessPipeline_);
            vkCmdDrawIndexedIndirect(cmd, tessIndirectBuffer_, 0, 1, sizeof(VkDrawIndexedIndirectCommand));
        }
    }

private:
    bool useMesh_ = false;
    VkBuffer explodeFragmentsBuffer_ = VK_NULL_HANDLE;
    VkBuffer dodgeOffsetsBuffer_ = VK_NULL_HANDLE;
    VkPipeline taskMeshPipeline_ = VK_NULL_HANDLE;
    VkPipeline tessPipeline_ = VK_NULL_HANDLE;
    VkBuffer indirectBuffer_ = VK_NULL_HANDLE;
    VkBuffer tessIndirectBuffer_ = VK_NULL_HANDLE;
};
```
