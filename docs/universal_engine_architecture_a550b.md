# Arquitectura del Motor Universal C++20 / Vulkan (compiz-gnome)

## 1. Visión General de la Arquitectura de Doble Capa

El proyecto `compiz-gnome` implementa una arquitectura híbrida no invasiva dividida en dos capas distintas según la naturaleza del procesamiento (CPU vs. GPU):

```
┌───────────────────────────────────────────────────────────────────────────┐
│                    ENTORNO HOST (GNOME Shell / Mutter)                     │
│                                                                           │
│   CAPA 1: Extensiones GJS / CPU (Gestión & Infraestructura)                │
│   - CompizWindowRules (regex, opacidad, fijado)                           │
│   - CompizTiling / Maximumize (algoritmos de espacio libre)               │
│   - CompizGroup & Shelf (pestañas y miniaturización)                      │
│   - Lock-Free Input Ring Buffer (memfd_create para Annotate/Showmouse)     │
└─────────────────────────────────────┬─────────────────────────────────────┘
                                      │ IPC (Unix Domain Socket / Protobuf)
                                      │ SCM_RIGHTS (DMA-BUF & Sync FDs)
                                      ▼
┌───────────────────────────────────────────────────────────────────────────┐
│              CAPA 2: Motor Headless C++20 / Vulkan (Efectos GPU)           │
│                                                                           │
│   - Frame Graph Engine & Render Pass Graph (Barrier2 Sync)                │
│   - Vulkan 1.3 Timeline Semaphores & DRM Format Modifiers                 │
│   - Wave Equation Compute Shader (Water)                                  │
│   - Mass-Spring Physics & Bézier Mesh (Wobbly)                            │
│   - 3D Elliptical Orbit & Z-Buffer (Ring Switcher)                        │
│   - Planar Reflection & Glossy Floor Shader (Shift Cover Flow)            │
│   - Tessellation & Mesh Shaders 1.3 (Magic Lamp, Curl, Explode)           │
│   - Descriptor Buffers (VK_EXT_descriptor_buffer)                         │
│                                                                           │
│   Exportación de Textura: DMA-BUF (VK_KHR_external_memory_fd)             │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Matriz de Clasificación Ejecutiva: CPU vs. GPU

| Plugin / Módulo | Nivel de Ejecución | Tecnología | Justificación Técnica |
| :--- | :--- | :--- | :--- |
| **Wobbly Windows** | 🔴 GPU (Vulkan) | Compute Shader / Vertex Grid | Simulación de masa-resorte por matriz de puntos (100+ nodos por ventana). |
| **3D Cube** | 🔴 GPU (Vulkan) | Graphics Pipeline (MVP) | Transformación 3D de escena con Skybox y deformación esférica. |
| **Burn / Fire** | 🔴 GPU (Vulkan) | Compute Shader (Partículas) | 50,000+ partículas con convección térmica y blend aditivo. |
| **Water** | 🔴 GPU (Vulkan) | Compute Shader (FDM 2D) | Ecuación de onda FDM en memoria compartida (18x18 LDS tile) + Ley de Snell. |
| **Shift / Cover Flow** | 🔴 GPU (Vulkan) | Graphics Pipeline + Compute | Reflexión planar $\mathbf{R}_{plane}$ y desenfoque de suelo Glossy $\sigma(y)$. |
| **Ring Switcher** | 🔴 GPU (Vulkan) | True 3D XZ Orbit | Órbita 3D real con ordenamiento de profundidad por Hardware Z-Buffer. |
| **Freewins** | 🔴 GPU (Vulkan) | Graphics Push Constants | Matriz 4x4 TRS + Skew. (Raycasting Ray-Plane para input en Wayland). |
| **Animation Suite** | 🔴 GPU (Vulkan) | Mesh Shaders 1.3 / Tess | Subdivisión dinámica en GPU (`VK_EXT_mesh_shader` para Magic Lamp/Curl/Explode). |
| **Blur** | 🔴 GPU (Vulkan) | Compute Shader | Gaussian separable + Dual Kawase Down/Upsampling. |
| **Scale / Expo** | 🔴 GPU (Vulkan) | Vertex Push Constants | Viewport scaling e interpolación de geometría de escritorios. |
| **Rotate / Wrot** | 🔴 GPU (Vulkan) | Vertex Push Constants | Acumulación de quaterniones y matrices de rotación Euler 3D. |
| **Zoom** | 🔴 GPU (Vulkan) | `vkCmdBlitImage` | Blit recortado de framebuffer con escalado bilinear/nearest. |
| **Wall / VSwipe** | 🔴 GPU (Vulkan) | Dynamic Scissor / Trans | Viewport sliding sobre mosaico de workspaces con resistencia elástica. |
| **Firepaint** | 🔴 GPU (Vulkan) | Particle SSBO Compute | Emisión e integración de partículas de fuego guiadas por el puntero. |
| **Motion Blur** | 🔴 GPU (Vulkan) | Accumulation Image | Mezcla temporal en textura de precisión float (`VK_FORMAT_R16G16B16A16`). |
| **3D Windows (td)** | 🔴 GPU (Vulkan) | Graphics Pipeline | Inclinación 3D $\mathbf{R}_x(\theta)$ y desenfoque Depth-of-Field proporcional a Z. |
| **ColorFilter / OBS / Neg** | 🔴 GPU (Vulkan) | Fragment Color Matrix | Multiplicación de espacio de color RGB/HSL por matriz $4\times 4$. |
| **Reflex** | 🔴 GPU (Vulkan) | Fragment Shader Overlay | Mapeo de textura de reflejo especular con degradado de alpha. |
| **Snap / Magnetic Edge** | 🟡 Híbrido (CPU+GPU) | JS Logic + GPU Approach | GJS elige snap target; GPU Compute anima el approach físico sin jank. |
| **Annotate / Showmouse** | 🟡 Híbrido (Input Lock-free) | `memfd` Ring Buffer + GPU | Captura de cursor en ring buffer `memfd` de baja latencia $\to$ GPU Compute Shader. |
| **Winrules / Regex** | 🟢 CPU (GJS) | JavaScript + Mutter API | Evaluación de expresiones regulares sobre propiedades de ventanas. |
| **Place / Put** | 🟢 CPU (GJS) | JavaScript + Mutter API | Aritmética de posicionamiento inicial y asignación de viewport. |
| **Maximumize** | 🟢 CPU (GJS) | Algoritmo Sweep-Line | Búsqueda en CPU del rectángulo libre más grande (expone VkBuffer de rects). |
| **Group (Pestañas)** | 🟢 CPU (GJS) | JavaScript + Clutter UI | Agrupación de contenedores y barra de pestañas visual sobre la ventana. |
| **Shelf (Estante)** | 🟢 CPU (GJS) | JavaScript + Clutter UI | Reducción de escala de ventana a miniatura flotante fija. |
| **Trailfocus** | 🟢 CPU (GJS) | Cola circular en JS | Seguimiento de orden de foco e interpolación de opacidad. |

---

## 3. Directrices Críticas de Producción y Sincronización (P0 / P1)

### A. Sincronización Explícita y Timeline Semaphores (`VK_KHR_timeline_semaphore`)
Para evitar **tearing, frame corruption o deadlocks** entre el motor Vulkan y Mutter:
1. **Timeline Semaphores**:
   - Motor C++: `vkQueueSubmit` emite señal a `timeline_semaphore` con valor $N$.
   - Exporta el manejador como `sync_fd` vía `VK_KHR_external_semaphore_fd`.
   - Se transfiere el trío `(texture_fd, sync_fd, timeline_value)` por IPC usando `SCM_RIGHTS`.
2. **Handshake con Mutter**:
   - Mutter ejecuta `eglWaitSyncKHR` sobre `sync_fd` antes del pass de composición.
   - Mutter devuelve un `release_sync_fd` cuando termina el scanout.
   - Motor C++ espera `release_sync_fd` antes de reutilizar el buffer.
3. **Triple Buffering**: El pool de imágenes Vulkan mantiene $N=3$ buffers por ventana para no bloquear la cola de presentación.

### B. DRM Format Modifiers (`VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT`)
- Para escaneo directo en hardware disimilitud (NVIDIA, AMD, Intel), el motor negocia `VkDrmFormatModifierPropertiesListEXT` al crear la `VkImage`.
- Previene blits costosos en CPU/GPU durante screencasts con PipeWire o direct scanout.

### C. Arquitectura Effect Graph (Render Graph en C++20)
El motor organiza los pases de renderizado mediante un **Render Graph** declarativo:
```cpp
template <typename T>
concept EffectNode = requires(T& node, FrameGraph& fg, FrameData& fd, vk::raii::CommandBuffer& cmd) {
    { node.setup(fg) } -> std::same_as<void>;
    { node.execute(cmd, fd) } -> std::same_as<void>;
};
```
- Deducción automática de barreras `VkImageMemoryBarrier2` en Vulkan 1.3.
- Reutilización de memoria transitoria para sub-pases de física y sombras.

### D. Descriptor Buffers (`VK_EXT_descriptor_buffer`)
Sustitución completa de `vkUpdateDescriptorSets` por **Descriptor Buffers** de Vulkan 1.3, vinculando descriptores con offsets en Push Constants para costo cero de CPU por ventana.

---

## 4. Aliasing de Recursos GPU (Transient Memory Aliasing Map)

El Frame Graph agrupa los recursos cuya vida útil no se solapa dentro del mismo frame para compartir memoria `VkDeviceMemory` mediante VMA (Vulkan Memory Allocator):

| Grupo de Aliasing | Consumidores de Pases Vulkan | Formato Recomendado | Memoria a 4K |
| :--- | :--- | :--- | :--- |
| `Transient_Color_0` | Water Sim (Height A), Burn (Front), Blur (Down 1) | `VK_FORMAT_R16_SFLOAT` / `R8G8B8A8_UNORM` | ~32 MB |
| `Transient_Color_1` | Water Sim (Height B), Blur (Down 2), Shift (Reflect) | `VK_FORMAT_R16_SFLOAT` / `R8G8B8A8_UNORM` | ~32 MB |
| `Transient_Normal` | Water Sim (Normal Map), Wobbly (Normal), CubeAddon | `VK_FORMAT_R16G16B16A16_SFLOAT` | ~64 MB |
| `Transient_Particle_SSBO` | Burn, Fire, Firepaint, Explode, Showmouse | `std430` struct (pos, vel, life, color) | ~1.6 MB |
| `Transient_Mesh_SSBO` | Animation Suite (Mesh 1.3 Output), Wobbly (Grid) | `std430` struct (pos, uv, normal) | Variable |

---

## 5. Estructura de Módulos C++20 Definitiva

```cpp
// engine.core.module.cpp
export module engine.core;
export import engine.vulkan;       // RAII wrappers, VMA, Timeline Semaphores
export import engine.framegraph;   // FrameGraph, ResourceHandle, PassNode
export import engine.math;         // FixedTimestep 120Hz, SpringDamper, WaveFDM
export import engine.ipc;          // Protobuf, Unix Domain Socket, SCM_RIGHTS
export import engine.window;       // WindowState, WindowID (uint64_t)

// engine.effects.module.cpp
export module engine.effects;
export import engine.effects.water;      // WaterComputePass, WaterRenderPass
export import engine.effects.wobbly;     // WobblyComputePass (Spring Grid)
export import engine.effects.cube;       // CubeRenderPass (Skybox, Sphere Morph)
export import engine.effects.burn;       // BurnComputePass (Particles)
export import engine.effects.shift;      // ShiftRenderPass (Planar Reflection)
export import engine.effects.ring;       // RingRenderPass (True 3D Orbit)
export import engine.effects.animation;  // AnimationMeshPass (Mesh Shaders 1.3)
```

---

## 6. Estrategia de Implementación: Vertical Slice MVP (Water Ripple Drag)

El plan de construcción del núcleo iniciará con el **Vertical Slice de Prueba**:

1. **GJS Extension**: Intercepta `motion-notify-event` al arrastrar una ventana $\to$ envía `WaterImpact(x, y, strength)` por IPC.
2. **C++20 Engine**: Instancia `WaterComputePass` dentro del `FrameGraph`. Ejecuta `water_sim.comp` a 120Hz fijos.
3. **DMA-BUF Roundtrip**: Exporta `normal_map_fd` $\to$ Mutter aplica `water_effect.frag` sin copias RAM/VRAM.
