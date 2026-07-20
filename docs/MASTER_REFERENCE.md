# Análisis de Arquitectura de Compiz y Mapeo a GNOME Moderno (Wayland)

## 1. Arquitectura Clásica de Compiz (X11)

Compiz se diseñó como un gestor de composición (*composite manager*) para X11 basado en OpenGL / GLX. Su arquitectura extensible se dividía en:

### Core (Núcleo)
- **`CompDisplay`**: Representa la conexión con el servidor X11. Maneja eventos globales (`XEvent`).
- **`CompScreen`**: Representa una pantalla física o virtual. Maneja el pipeline de renderizado (`paintOutput`).
- **`CompWindow`**: Representa una ventana administrada. Maneja geometría, textura XComposite y hooks de renderizado (`paintWindow`, `damage`).

### Sistema de Plugins y Hooks
Compiz extendía el comportamiento reemplazando punteros a funciones vtable en tiempo de ejecución:
- `screen->paintOutput()`: Inicia el frame de renderizado.
- `window->paintWindow()`: Dibuja cada ventana. Los plugins alteran matrices de transformación, vértices o shaders.
- **Deformación Geométrica (Mesh Grids)**: Plugins como `Wobbly` subdividían la textura en una malla 2D de vértices y aplicaban física de masa-resorte (*spring-damper*).
- **Proyección 3D (Scene Graph / Skybox)**: Plugins como `Cube` proyectaban los escritorios virtuales sobre las caras de un prisma 3D o esfera.

---

## 2. Mapeo a GNOME Moderno (Wayland + Mutter)

En entornos Wayland modernos (GNOME Shell / Mutter), no existe un gestor de composición desacoplado X11. Mutter actúa como servidor Wayland y compositor único dentro del proceso principal de GNOME Shell.

### Arquitectura Híbrida de Doble Capa (`compiz-gnome`)

Para migrar la experiencia de Compiz sin comprometer la estabilidad del sistema, `compiz-gnome` adopta una **Arquitectura de Doble Capa**:

1. **Capa 1: Extensión GJS de GNOME Shell (Ligera & In-Process)**:
   - Intercepta los `MetaWindowActor` (ClutterActor) de Mutter.
   - Extrae el buffer de textura de la ventana en formato **DMA-BUF** (`zwp_linux_dmabuf_v1` / `CoglTexture`).
   - Envía handles IPC (`dmabuf_fd`, `drm_format`, `modifier`, `acquire_sync_fd`) al motor C++.
   - Aplica un `ClutterEffect` de renderizado passthrough directo (`gl_FragColor = texture(engine_output_dmabuf, uv)`).

2. **Capa 2: Motor Headless C++20 / Vulkan (Aislado & Offscreen)**:
   - Recibe la textura DMA-BUF mediante `VK_KHR_external_memory_fd`.
   - Procesa la física de deformación (Wobbly), ecuaciones de onda (Water), transformaciones 3D (Cube/Ring/Shift) en **Vulkan Compute & Graphics Pipelines**.
   - Exporta el resultado de vuelta como una textura DMA-BUF sin copias entre memoria RAM/VRAM (**Zero-Copy Real**).

---

## 3. Manejo de Identidad de Ventana y Sincronización

### A. Identificador Estable de Ventana (`uint64_t WindowID`)
Los punteros de `MetaWindowActor` en Clutter se destruyen dinámicamente durante unminimize o cambio de workspace.
- **Fuente de verdad**: `uint64_t WindowID` basado en `MetaWindow::get_id()` o hash estable.
- **Estado del Motor C++**: El motor C++ mantiene un `std::unordered_map<WindowID, WindowState>` con las mallas de masa-resorte, SSBOs de partículas e historia de simulación.

### B. Uso de Wayfire como Referencia Técnica
- **Uso exclusivo**: Referencia de **fórmulas matemáticas puras** (ecuaciones de resortes, matrices MVP, transformaciones elípticas).
- **NO reutilizado**: La gestión de textura EGL/FBO de Wayfire, binds implícitos GL y punteros crudos se sustituyen por **Vulkan 1.3 Data-Oriented Design (DoD)**, `VmaAllocation`, `vk::raii` y `VkTimelineSemaphore`.

---

## 4. Detalles de Implementación de Producción en Wayland/Vulkan

### 4.1. Intercambio de Texturas: DMA-BUF + Modifiers + Sync FD
- **Capa 1 (GJS)** obtiene `MetaTexture` / `CoglTexture` $\to$ Extrae `dmabuf_fd` + `drm_format` + `modifier` (vía `EGL_EXT_image_dma_buf_import_modifiers`).
- **IPC (Protobuf + SCM_RIGHTS)** transmite: `fd`, `width`, `height`, `stride`, `format`, `modifier`, `acquire_sync_fd`.
- **Capa 2 (C++/Vulkan)** importa la imagen con `VkImportMemoryFdInfoKHR` y sincroniza con `VkImportSemaphoreFdInfoKHR` (Timeline Semaphore).

### 4.2. Modelo de Hilos (Threading Model)
- **Hilo Principal GJS (UI Thread)**: Solo IPC no bloqueante (`send()`, `poll()`). **Cero llamadas Vulkan.**
- **Hilo Render Motor C++**: `vkQueueSubmit2` con Timeline Semaphores $\to$ Señala `release_sync_fd` para el scanout de Mutter.

### 4.3. Manejo de HiDPI / Fractional Scaling
- Mutter compone en **coordenadas lógicas**. Vulkan renderiza en **píxeles físicos**.
- El IPC incluye `float scale_factor` por ventana/monitor.
- Parámetros de física (radio de onda, rigidez de resorte) se escalan por `scale_factor` en CPU antes de Push Constants.
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
# Análisis Comparativo: Los 50 Plugins de Infraestructura de Compiz vs. GNOME Shell / Wayland

## 1. El Dilema de Infraestructura: Compiz X11 vs. GNOME Shell Moderno

Compiz (2006-2012) incluía unos ~50 plugins dedicados a la infraestructura de ventanas en X11 (`winrules`, `place`, `put`, `snap`, `decor`, `dbus`, `regex`, `annotate`, `showmouse`, `group`, `shelf`, etc.). 

Existe la duda común de si GNOME Shell moderno en Wayland reemplaza completamente estos plugins o si su implementación es superior o inferior a la de Compiz.

---

## 2. Comparativa Detallada por Categoría

### A. Reglas de Ventana y Posicionamiento (`winrules`, `regex`, `place`, `put`)
- **Compiz (2007)**: Sistema potente basado en expresiones regulares (regex) sobre título, clase X11, rol o tipo de ventana. Permitía fijar posición exacta por píxel, workspace asignado, opacidad inicial, estado de la ventana (siempre visible, al fondo, sin bordes) y pantalla de salida.
- **GNOME Shell (Moderno)**: Es extremadamente opinado y básico. Sutter/Mutter ubica las ventanas de forma centrada o recordada de forma muy limitada. **No existe motor de reglas regex nativo en GNOME Shell**.
- **Veredicto**: **Compiz era inmensamente superior**. Hoy en día los usuarios de GNOME deben instalar extensiones de terceros (como *Auto Move Windows* o *Window Rules*) que son fragmentadas.
- **Oportunidad de Extensión en `compiz-gnome`**: Crear un módulo GJS `CompizWindowRules` que lea expresiones regulares y aplique estados mediante la API de Mutter/Clutter.

### B. Snapping y Tiling de Ventanas (`snap`, `grid`, `maximumize`)
- **Compiz (2007)**: Atracción magnética por bordes de ventanas adyacentes (`snap`), cuadrículas del teclado numérico (`grid`) y algoritmo de expansión de área máxima libre (`maximumize`).
- **GNOME Shell (Moderno)**: Incluye snapping básico a la mitad izquierda/derecha.
- **Veredicto**: **Compiz tenía características únicas** como `maximumize` (expandir ventana hasta tocar las vecinas sin cubrir ninguna) que GNOME no tiene nativamente.
- **Oportunidad de Extensión**: Implementar el algoritmo sweep-line de `maximumize` en GJS.

### C. Personalización Visual y Decoraciones (`decor` / Emerald)
- **Compiz (2007)**: Decorador externo desacoplado (`gtk-window-decorator` / `emerald`) que permitía bordes de cristal transparente, botones personalizados, sombras dinámicas y esquemas de color independientes de la app.
- **GNOME Shell (Moderno)**: Usa Client-Side Decorations (CSD) con GTK4/Libadwaita o Server-Side Decorations (SSD) cerradas en Mutter. La personalización de bordes está fuertemente restringida por diseño.
- **Veredicto**: **Compiz Emerald era inalcanzable en personalización visual**.
- **Oportunidad de Extensión**: En nuestro motor Vulkan, la capa de sombras y bordes brillantes/animados se puede inyectar sobre el contenedor del actor de ventana en Clutter.

### D. Herramientas Interactivas de Pantalla (`annotate`, `showmouse`, `group`, `shelf`)
- **Compiz (2007)**:
  - `annotate`: Dibujar libremente en pantalla sobre cualquier ventana con atajo de teclado + ratón.
  - `showmouse`: Destello / rastro de partículas o anillos al presionar una tecla para localizar el cursor.
  - `group`: Agrupar cualquier conjunto de ventanas en un contenedor con pestañas superiores.
  - `shelf`: Minimizar ventanas como "estantes" flotantes escalados en cualquier esquina del escritorio.
- **GNOME Shell (Moderno)**: **Ninguna de estas características existe en GNOME Shell por defecto.**
- **Veredicto**: **Compiz ofrecía utilidades de productividad adelantadas a su tiempo.**
- **Oportunidad de Extensión**: Implementar `CompizAnnotate` (dibujo en superficie Vulkan/Clutter) y `CompizGroup` (módulos GJS con gran valor de uso).

---

## 3. Resumen del Gap y Hoja de Ruta para Extensiones GJS

| Plugin de Compiz | Disponible en GNOME Shell Nativo? | Superior / Inferior | Mapeo como Extensión `compiz-gnome` |
| :--- | :--- | :--- | :--- |
| `winrules` / `regex` | ❌ No | Compiz superior | Extensión GJS de Reglas Regex |
| `maximumize` | ❌ No | Compiz superior | Módulo GJS Sweep-Line |
| `annotate` | ❌ No | Compiz superior | Overlay Shader en Vulkan/GJS |
| `showmouse` | ❌ No (solo accesibilidad básica) | Compiz superior | Shader de partículas alrededor de cursor |
| `group` (Pestañas) | ❌ No | Compiz superior | Decorador GJS de Pestañas |
| `shelf` | ❌ No | Compiz superior | Extensión de Thumbnails flotantes |
| `emerald` (Decorador) | ❌ Restringido por Adwaita | Compiz superior | Inyección de Bordes/Shaders en Clutter |

---

## 4. Conclusión

Los 50 plugins de infraestructura de Compiz **no han sido superados por GNOME Shell**; al contrario, la simplicidad de GNOME dejó vacíos que hoy se pueden llenar convirtiendo esos antiguos plugins en **extensiones GJS modulares y elegantes** impulsadas por nuestro motor C++20/Vulkan.
# Análisis Matemático y Modelo Físico: Wobbly Windows (Ventanas Gelatinosas)

## 1. Fundamentos Físicos

El efecto de *Wobbly Windows* (ventanas gelatinosas) emula el comportamiento de un material elástico mediante una red de **masa-resorte-amortiguador** (Spring-Damper Mesh Grid) combinada con **Superficies de Bézier Bi-cúbicas**.

```
  (O_0,0)───[Spring]───(O_1,0)───[Spring]───(O_2,0)
     │                    │                    │
  [Spring]             [Spring]             [Spring]
     │                    │                    │
  (O_0,1)───[Spring]───(O_1,1)───[Spring]───(O_2,1)
```

---

## 2. Ecuaciones Diferenciales Discretizadas

Cada objeto de la malla $O_i$ tiene posición $\vec{r}_i$, velocidad $\vec{v}_i$ y masa $m$ ($WOBBLY\_MASS = 15.0$).

### A. Ley de Hooke (Fuerza Elástica del Resorte)
Para dos objetos conectados $a$ y $b$ con desplazamiento ideal de reposo $\vec{d}_{offset}$:

$$\vec{\Delta r} = \frac{1}{2} \cdot (\vec{r}_b - \vec{r}_a - \vec{d}_{offset})$$

$$\vec{F}_{spring\_a} = k \cdot \vec{\Delta r}$$
$$\vec{F}_{spring\_b} = -k \cdot \vec{\Delta r}$$

Donde $k$ ($MINIMAL\_SPRING\_K = 0.1$, $MAXIMAL\_SPRING\_K = 10.0$) es la constante de rigidez.

### B. Amortiguamiento Viscoso (Fricción)
Para evitar oscilaciones infinitas, se aplica una fuerza de arrastre opuesta a la velocidad:

$$\vec{F}_{friction} = -\mu \cdot \vec{v}_i$$

Donde $\mu$ es la constante de fricción ($MINIMAL\_FRICTION = 0.1$, $MAXIMAL\_FRICTION = 10.0$).

### C. Integración Temporal (Euler / Verlet)
En cada paso del bucle de simulación ($\Delta t \approx 16ms$):

$$\vec{a}_i = \frac{\vec{F}_{total}}{m} = \frac{\vec{F}_{spring} + \vec{F}_{friction}}{m}$$

$$\vec{v}_i(t + \Delta t) = \vec{v}_i(t) + \vec{a}_i \cdot \Delta t$$

$$\vec{r}_i(t + \Delta t) = \vec{r}_i(t) + \vec{v}_i(t + \Delta t) \cdot \Delta t$$

---

## 3. Interpolación Bézier Bi-cúbica (Smooth Surface Patch)

Para evitar que la ventana se vea como un polígono rígido de $4 \times 4$ puntos, los vértices finales del renderizado se calculan evaluando un **Parche de Bézier de orden 3**:

$$P(u, v) = \sum_{i=0}^{3} \sum_{j=0}^{3} B_i^3(u) \cdot B_j^3(v) \cdot \vec{r}_{i,j}$$

Donde los polinomios de Bernstein son:
- $B_0^3(t) = (1 - t)^3$
- $B_1^3(t) = 3t(1 - t)^2$
- $B_2^3(t) = 3t^2(1 - t)$
- $B_3^3(t) = t^3$

Para cualquier punto normado $(u, v) \in [0, 1] \times [0, 1]$ de la textura de la ventana, $P(u, v)$ retorna la coordenada deformada $(x', y')$.

---

## 4. Implementación en C++20 / Vulkan

En el motor `compiz-gnome`:
1. La simulación de los $N \times M$ resortes se puede ejecutar de dos formas:
   - **CPU**: En un hilo dedicado (`std::jthread`) en C++20 usando `std::vector` alineado con SIMD (AVX2).
   - **GPU**: Mediante un **Compute Shader en Vulkan** que calcula las fuerzas de todos los nodos en paralelo.
2. Las coordenadas deformadas se pasan directamente a un **Vertex Shader de Vulkan** para proyectar la textura de la ventana.
# Análisis Matemático y Transformaciones 3D: Desktop Cube (Cubo de Escritorio)

## 1. Geometría Espacial del Cubo de $N$ Caras

El plugin de Cubo 3D proyecta $N$ escritorios virtuales (*workspaces*) sobre las caras laterales de un prisma poligonal regular de $N$ lados.

```
                  Top View (Vista Superior N=4)
                        Face 1
                    ┌────────────┐
                    │            │
         Face 0     │  (0,0,0)   │    Face 2
                    │     •      │
                    └────────────┘
                        Face 3
```

---

## 2. Ángulos y Distancia al Centro (Identity Z-Offset)

### A. Ángulo por Cara ($\theta_{side}$)
Para $N$ escritorios en la grilla horizontal:

$$\theta_{side} = \frac{2\pi}{N}$$

- Para $N=4$ caras (Cubo perfecto): $\theta_{side} = \frac{\pi}{2} = 90^\circ$.
- Para $N=6$ caras (Hexágono): $\theta_{side} = 60^\circ$.

### B. Distancia al Centro ($z_{identity}$)
La distancia desde el centro del prisma a cada cara para que el ancho de la cara encaje exactamente en la pantalla es:

$$z_{identity} = \frac{0.5}{\tan\left(\frac{\theta_{side}}{2}\right)}$$

- Para $N=4$: $z_{identity} = \frac{0.5}{\tan(45^\circ)} = 0.5$.

---

## 3. Matrices de Transformación (Model-View-Projection)

### A. Matriz Modelo de la Cara $i$ ($M_i$)
Para la cara $i \in \{0, 1, \dots, N-1\}$ con una rotación acumulada del usuario $\theta_{user}$:

$$\theta_i = i \cdot \theta_{side} + \theta_{user}$$

$$M_i = R_y(\theta_i) \cdot T\left(0, 0, z_{identity} + z_{extra}\right)$$

Donde la matriz de rotación en el eje $Y$ es:

$$R_y(\theta) = \begin{pmatrix} \cos\theta & 0 & \sin\theta & 0 \\ 0 & 1 & 0 & 0 \\ -\sin\theta & 0 & \cos\theta & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

### B. Matriz de Vista ($V$)
Maneja el zoom del usuario ($z_{zoom}$), la inclinación vertical (pitch $\phi_y$) y la cámara:

$$V = T(0, 0, -z_{offset}) \cdot R_x(\phi_y) \cdot LookAt\left((0,0,0), (0,0,-z_{offset}), (0,1,0)\right)$$

### C. Matriz de Proyección ($P$)
Matriz de perspectiva estándar:

$$P = glm::perspective\left(FOV = 45^\circ, aspect = \frac{width}{height}, z_{near} = 0.1, z_{far} = 100.0\right)$$

### D. Matriz Final MVP
$$\text{MVP}_i = P \cdot V \cdot M_i$$

---

## 4. Implementación en C++20 / Vulkan

En nuestro motor `compiz-gnome`:
1. **GLM (OpenGL Mathematics)** o una librería de álgebra lineal nativa en C++20 calcula las matrices $P, V, M_i$.
2. **Culling por Cintas de Caras (Backface Culling / Two-Pass Winding)**:
   - Se renderizan primero las caras traseras ($GL\_CCW$) para el interior del cubo/skybox.
   - Luego se renderizan las caras frontales ($GL\_CW$) con soporte para transparencia Alpha.
3. **Vulkan Push Constants / Uniform Buffers**:
   - Cada frame actualiza un `UniformBufferObject` con la matriz `MVP` enviada directamente a la pipeline de Vulkan.
# Análisis Matemático y Algoritmo: Burn Effect (Efecto de Quemar Ventanas)

## 1. Fundamentos del Efecto de Combustión

El efecto de **Burn** (desintegración por fuego) simula una línea de combustión vertical que avanza a lo largo de la superficie de la ventana, combinando dos componentes:
1. **Recorte Progresivo de Geometría (Scissoring / Clipping Line)**.
2. **Sistema de Partículas Estocástico de Fuego (Particle System & Heat Convection)**.

```
       ┌───────────────────────────┐
       │   Ventana Intacta         │
       │                           │
  ═════╪═══════════════════════════╪═════ Linea de Fuego (progress_line)
       │  🔥  ✨  🔥  ✨  🔥  ✨  │ Emisión de Partículas (Heat buoyancy)
       │     (Área Quemada)        │
       └───────────────────────────┘
```

---

## 2. Frente de Combustión y Recorte

Sea $H$ la altura de la ventana y $P(t) \in [0, 1]$ la función de progreso en el tiempo $t$:

$$Y_{fire}(t) = H \cdot P(t)$$

El área visible de la ventana se recorta dinámicamente limitando la representación geométrica a la región $[0, Y_{fire}(t)]$.

---

## 3. Dinámica del Sistema de Partículas

En cada cuadro de renderizado, se emiten $N_p$ partículas a lo largo del frente $Y_{fire}(t)$.

### A. Inicialización de Partícula
Para cada partícula $p$:
- **Posición Inicial**: $x_0 \sim U(0, W)$, $y_0 \sim U(Y_{fire} - \Delta y, Y_{fire} + \Delta y)$
- **Velocidad Inicial**: $\vec{v}_0 = (v_x, v_y)$ donde $v_x \sim U(-10, 10)$, $v_y \sim U(-25, 5)$
- **Aceleración por Convección Térmica (Flotabilidad)**: $\vec{g} = (-1, -3)$ (el calor empuja las llamas hacia arriba y ligeramente a un lado).
- **Vida y Decaimiento**: $\text{life}_0 = 1.0$, $\text{fade} \sim U(0.1, 0.6)$

### B. Ecuaciones de Movimiento e Integración
En cada paso de tiempo $\Delta t$:

$$\vec{v}(t + \Delta t) = \vec{v}(t) + \vec{g} \cdot \Delta t$$

$$\vec{r}(t + \Delta t) = \vec{r}(t) + \vec{v}(t + \Delta t) \cdot \Delta t$$

$$\text{life}(t + \Delta t) = \text{life}(t) - \text{fade} \cdot \Delta t$$

$$\text{radius}(t) = \text{radius}_0 \cdot \text{life}(t)$$

### C. Modelo de Color (Fuego / Ceniza)
El color de la partícula varía dinámicamente con la temperatura/vida:

$$C = (R, G, B, A)$$

Para una paleta de fuego estándar:
- $R \sim U(0.8, 1.0)$ (Rojo intenso)
- $G \sim U(0.3, 0.7)$ (Amarillo/Naranja)
- $B \sim U(0.0, 0.2)$ (Azul/Poco aporte)
- Alpha: $A(t) = \text{life}(t)$

---

## 4. Implementación en C++20 / Vulkan

En nuestro motor `compiz-gnome`:
1. **Compute Shader de Partículas en Vulkan**:
   - Actualiza en paralelo las posiciones, velocidades y vidas de 10,000+ partículas directamente en VRAM usando un **Storage Buffer (SSBO)**.
2. **Renderizado de Partículas con Mezcla Aditiva**:
   - `vkCmdDraw` instancia cada partícula como un Sprite / Quad orientado a la cámara.
   - El pipeline utiliza mezcla aditiva (`VK_BLEND_FACTOR_SRC_ALPHA`, `VK_BLEND_FACTOR_ONE`) para dar el brillo característico del fuego.
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
# Análisis Matemático: Scale/Expo (Vista General de Workspaces)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/expo.cpp`
- `compiz-0.9.14.2/plugins/scale/src/scale.cpp`
- `compiz-0.9.14.2/plugins/expo/src/expo.cpp`

---

## 1. Fundamento Conceptual

El plugin **Expo/Scale** presenta todos los workspaces (o ventanas) simultáneamente como una cuadrícula escalada al tamaño de la pantalla, permitiendo al usuario navegar visualmente entre ellos.

---

## 2. Transformación de Viewport (Expo)

### A. Grid de Workspaces
Sea la grilla de escritorios de $N_w \times N_h$ workspaces, cada uno de resolución $W \times H$ píxeles.

El **Wall Virtual** (pared completa de workspaces) tiene dimensiones:
$$W_{wall} = N_w \times W + (N_w + 1) \times gap$$
$$H_{wall} = N_h \times H + (N_h + 1) \times gap$$

### B. Animación de Zoom-Out (Viewport Scaling)
La transición se maneja mediante una **animación de geometría interpolada** entre dos rectángulos viewport:

- **Estado inicial** (activo en un workspace): El viewport es un rectángulo del tamaño exacto del workspace actual.
- **Estado final** (expo activo): El viewport es un rectángulo que engloba **toda la pared** de workspaces más un margen centrado para igualar la relación de aspecto:

$$W_{full} = (gap + W) \times \max(N_w, N_h) + gap$$
$$H_{full} = (gap + H) \times \max(N_w, N_h) + gap$$

El centrado del wall en pantalla se realiza ajustando la posición x/y del viewport:
$$x_{rect} -= \frac{W_{full} - W_{wall}}{2}, \quad y_{rect} -= \frac{H_{full} - H_{wall}}{2}$$

### C. Interpolación Temporal (Lerp Animado)
En cada frame, la posición y tamaño del viewport se interpola suavemente entre start y end:

$$viewport(t) = lerp(viewport_{start}, viewport_{end}, ease(t))$$

Donde `ease(t)` es una curva de suavizado (típicamente cúbica o cuadrática).

---

## 3. Scale: Transformaciones Afines de Ventanas

El plugin **Scale** (dentro del contexto de un workspace) aplica una transformación afín 2D a cada ventana para que quepa en su celda asignada de la cuadrícula.

### A. Algoritmo de Asignación de Ventanas (Slot Assignment)
Compiz usa un algoritmo de empaquetado que divide la pantalla en columnas y asigna ventanas de arriba a abajo, optimizando el espacio desperdiciado (similar a un algoritmo *bin-packing greedy*).

### B. Transformación Afín por Ventana
Para una ventana $i$ con bounding box $(bx_i, by_i, bw_i, bh_i)$ y slot asignado $(sx_i, sy_i, sw_i, sh_i)$:

**Factor de escala uniforme** (mantiene aspecto):
$$s_i = \min\left(\frac{sw_i}{bw_i}, \frac{sh_i}{bh_i}\right)$$

**Traslación para centrar en el slot**:
$$\Delta x_i = sx_i + \frac{sw_i - bw_i \cdot s_i}{2} - bx_i$$
$$\Delta y_i = sy_i + \frac{sh_i - bh_i \cdot s_i}{2} - by_i$$

**Matriz de transformación 2D homogénea**:
$$M_i = \begin{pmatrix} s_i & 0 & \Delta x_i \\ 0 & s_i & \Delta y_i \\ 0 & 0 & 1 \end{pmatrix}$$

### C. Highlight de Workspace Activo
Cada workspace tiene una animación de atenuación de brillo (`ws_fade`):
- **Workspace seleccionado**: brightness $= 1.0$
- **Workspaces no seleccionados**: brightness $= inactive\_brightness$ (ej. $0.7$)

El color se multiplica en el fragment shader: `fragColor.rgb *= brightness`.

---

## 4. Implementación en C++20 / Vulkan

En nuestro motor:
1. La animación de viewport se implementa con una interpolación lineal suave en el Vertex Shader usando matrices de proyección ortogonal 2D.
2. El escalado de ventanas se gestiona con **Push Constants** de Vulkan actualizados por frame: `{scale_x, scale_y, offset_x, offset_y}`.
3. El atenuado de brightness se aplica como un multiplicador uniforme en el Fragment Shader.
# Análisis Matemático: Switcher 3D (Navegador de Ventanas Alt-Tab)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/switcher.cpp`
- `compiz-0.9.14.2/plugins/switcher/src/switcher.cpp`
- `compiz-0.9.14.2/plugins/ring/src/ring.cpp`
- `compiz-0.9.14.2/plugins/shift/src/shift.cpp`

---

## 1. Modelo de Posicionamiento de 3 Slots

El switcher organiza las ventanas en un carrusel de 3 posiciones relativas:

```
  LEFT (-1)          CENTER (0)         RIGHT (+1)
┌──────────┐     ┌──────────────┐     ┌──────────┐
│  Vista   │     │ Vista Activa │     │  Vista   │
│ (atrás)  │     │ (al frente)  │     │ (atrás)  │
└──────────┘     └──────────────┘     └──────────┘
off_x < 0         off_x = 0           off_x > 0
off_z = -1.0      off_z = 0.0         off_z = -1.0
scale = 0.66      scale = 1.0         scale = 0.66
```

---

## 2. Transformaciones Animadas por Slot

Cada vista tiene un conjunto de atributos animados (`SwitcherPaintAttribs`) con transiciones temporizadas:

| Atributo | Centro (Activo) | Izquierda/Derecha (Secundario) |
| :--- | :--- | :--- |
| `off_x` | $0$ | $\pm W / 3$ |
| `off_z` | $0$ | $-1.0$ |
| `scale_x = scale_y` | $s_{fit}$ | $s_{fit} \times 0.66$ |
| `rotation` (eje Y) | $0$ | $\pm \theta_{rot}$ |
| `alpha` | $1.0$ | $0.7$ |

### A. Factor de Escala para Ajuste a Pantalla
Cada ventana se escala para ocupar como máximo el $45\%$ del ancho o alto de la pantalla:
$$s_{fit} = \min\left(\frac{0.45 \cdot W_{screen}}{bw_{view}}, \frac{0.45 \cdot H_{screen}}{bh_{view}}, 1.0\right) \times thumbnail\_scale$$

### B. Traslación Lateral al Centro de la Pantalla
Para centrar la ventana en la posición CENTER antes de cualquier desplazamiento lateral:
$$\Delta x = \frac{W_{screen}}{2} - \frac{bw_{view}}{2} - bx_{view}$$
$$\Delta y = by_{view} - \left(\frac{H_{screen}}{2} - \frac{bh_{view}}{2}\right)$$

### C. Movimiento entre Slots (Dirección $d \in \{-1, +1\}$)
Al presionar Tab, cada vista se desplaza en la dirección $d$:

$$\Delta x_{new} = \Delta x_{old} + \frac{W_{screen}}{3} \cdot d$$

**Cambio de profundidad $Z$** según el movimiento:
- Si la vista sale del **centro** → sube en Z ($z\_sign = +1$): $\Delta z_{new} = \Delta z_{old} + (-1.0)$
- Si la vista **llega al centro** desde lateral → baja en Z ($z\_sign = -1$): $\Delta z_{new} = \Delta z_{old} + (+1.0)$
- Si la vista **expira** (sale del rango) → $z\_sign = 0$

**Escala al moverse**:
$$s_{new} = s_{old} \times 0.66^{z\_sign}$$

**Rotación en eje Y** (en radianes):
$$\theta_{new} = \theta_{old} + \theta_{rot} \cdot d \quad \text{donde } \theta_{rot} = -\text{view\_thumbnail\_rotation} \cdot \frac{\pi}{180}$$

---

## 3. Matriz de Transformación 3D Final por Vista

La transformación completa de cada vista en el switcher combina:

$$M_{vista} = T(\Delta x, \Delta y, \Delta z) \cdot S(s_x, s_y, 1.0) \cdot R_y(\theta)$$

Donde en GLM (tal como lo implementa Wayfire):
```cpp
transform->translation = glm::translate(glm::mat4(1.0), {off_x, off_y, off_z});
transform->scaling     = glm::scale(glm::mat4(1.0), {scale_x, scale_y, 1.0});
transform->rotation    = glm::rotate(glm::mat4(1.0), rotation, {0.0, 1.0, 0.0});
```

---

## 4. Ordenamiento de Profundidad (Z-Sort sin Depth Buffer)
Para vistas con valores $Z$ similares se usa el **orden de apilamiento** de ventanas del sistema:
- Vistas con mayor $Z$ (más cercanas) se renderizan **después** (por encima).
- Al no usar depth testing, se ordena con un bubble-sort estable.

---

## 5. Implementación en C++20 / Vulkan

En nuestro motor:
1. Cada ventana se renderiza con sus propias **Push Constants** de translación, escala y rotación.
2. Las interpolaciones temporales de los atributos se implementan como `std::lerp` en C++20 sincronizado con el timestamp del frame (`VkSubmitInfo`).
3. El ordenamiento Z se hace en CPU antes de cada `vkCmdDraw`.
# Análisis Matemático: Rotate y Wrot (Rotación 2D y 3D Libre de Ventanas)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/wrot.cpp`
- `compiz-0.9.14.2/plugins/rotate/src/rotate.cpp`

---

## 1. Modos de Rotación

El plugin `wrot` (Window Rotation) implementa dos modos distintos de transformación:

| Modo | Eje de Rotación | Tipo de Transformación |
| :--- | :--- | :--- |
| **ROT_2D** | Eje Z (plano de la pantalla) | `view_2d_transformer_t` |
| **ROT_3D** | Eje arbitrario en 3D | `view_3d_transformer_t` |

---

## 2. Rotación 2D (Plano de Pantalla, Eje Z)

### Fundamento: Ángulo del Producto Vectorial
Cuando el usuario arrastra el ratón alrededor del centro de la ventana, el ángulo de rotación acumulado se calcula usando el **producto vectorial** entre el vector anterior y el actual:

Sea $\vec{v_1} = (x_1 - c_x, y_1 - c_y)$ y $\vec{v_2} = (x_2 - c_x, y_2 - c_y)$ los vectores desde el centro $(c_x, c_y)$ al cursor:

$$\theta_{delta} = -\arcsin\left(\frac{\vec{v_1} \times \vec{v_2}}{|\vec{v_1}| \cdot |\vec{v_2}|}\right) = -\arcsin\left(\frac{x_1 y_2 - x_2 y_1}{|\vec{v_1}| \cdot |\vec{v_2}|}\right)$$

El ángulo acumulado de la ventana se actualiza:
$$\theta_{total} = \theta_{total} + \theta_{delta}$$

Dado que $|\vec{v_1}|$ y $|\vec{v_2}|$ están normalizados, $\arcsin$ produce el ángulo exacto de giro entre los dos vectores en cada frame.

**Reset automático**: Si el cursor cae dentro de un radio `reset_radius` del centro, la transformación se elimina.

---

## 3. Rotación 3D Libre (Eje Arbitrario)

### Fundamento: Quaterniones / Rotation Accumulation (Rodrigues)
En modo 3D, el movimiento del ratón $(dx, dy)$ define un **eje de rotación** en el plano de la pantalla perpendicular al desplazamiento y una **magnitud de ángulo** proporcional a la velocidad:

$$\vec{axis} = (dir \cdot dy, dir \cdot dx, 0)$$

$$\theta_{delta} = |\vec{v}_{mouse}| \cdot ascale \quad \text{donde } ascale = \frac{sensitivity}{60} \cdot \frac{\pi}{180}$$

La rotación se acumula multiplicando la matriz de rotación existente:
```cpp
rotation = glm::rotate(rotation, vlen(dx, dy) * ascale, {dir*dy, dir*dx, 0});
```

Esto es equivalente a la acumulación de quaterniones de rotación compuesta:
$$Q_{total} = Q_{delta} \otimes Q_{total}$$

### Protección Anti-Singular (Normal Dot Product)
Al soltar el ratón, se verifica si la ventana quedó perpendicular a la pantalla (normal $\vec{n} = (0,0,1)$):
$$\cos(\alpha) = \vec{n} \cdot (Q_{total} \cdot \vec{n})$$

Si $|\cos(\alpha)| < 0.05$ (la ventana está casi de canto), se aplica una pequeña corrección de $\pm 2.5°$ para evitar que la ventana quede "atrapada" en posición invisible.

---

## 4. Implementación en C++20 / Vulkan

1. **Rotación 2D**: Uniform Buffer `{angle_radians}` → Fragment Shader aplica `mat2(cos, -sin, sin, cos)` sobre las coordenadas UV de la textura.
2. **Rotación 3D**: Uniform Buffer `{rotation_matrix_4x4}` → Vertex Shader multiplica la posición de cada vértice.
3. Ambos usan **Push Constants** en Vulkan por su tamaño pequeño y acceso rápido.
# Análisis Matemático: Zoom de Pantalla (Magnificación Digital)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/zoom.cpp`
- `compiz-0.9.14.2/plugins/ezoom/src/zoom.cpp`

---

## 1. Modelo de Magnificación

El zoom de pantalla implementa una **magnificación digital** del framebuffer final antes de la presentación. Es un efecto **post-process** (opera sobre el framebuffer completo, no sobre ventanas individuales).

---

## 2. Transformación de Viewport (Blit Recortado)

El truco fundamental es **renderizar una región reducida del framebuffer al tamaño completo de la pantalla**. Esto produce el efecto de ampliación sin alterar la geometría de ninguna ventana.

### A. Factor de Zoom
El factor de magnificación $f \in [1, 50]$ se actualiza con la rueda del ratón:
$$f_{new} = f_{old} - f_{old} \cdot \delta \cdot speed$$

Donde $\delta$ es el desplazamiento de la rueda. Esta fórmula produce un **zoom multiplicativo** (proporcional al nivel actual), lo que hace que los saltos de zoom sean perceptualmente uniformes tanto en valores bajos como altos.

### B. Coordenadas del Rectángulo de Origen (Source Region)
Partiendo de la posición del cursor $(c_x, c_y)$ en coordenadas de framebuffer:

**Escala de compensación:**
$$scale = \frac{f - 1}{f}$$

**Esquina superior izquierda del rectángulo de lectura:**
$$x_1 = \text{clamp}(c_x \cdot scale, \; 0, \; W-1)$$
$$y_1 = \text{clamp}(c_y \cdot scale, \; 0, \; H-1)$$

**Ancho y Alto del rectángulo de lectura:**
$$tw = \text{clamp}(W / f, \; 0, \; W - x_1)$$
$$th = \text{clamp}(H / f, \; 0, \; H - y_1)$$

**Interpretación**: Se lee una región de $tw \times th$ píxeles (más pequeña que la pantalla completa) centrada cerca del cursor, y se estira para llenar toda la pantalla de $W \times H$ píxeles.

### C. Ejemplo Numérico
Para una pantalla de $1920 \times 1080$, cursor en $(960, 540)$, factor $f = 2$:
- $scale = (2-1)/2 = 0.5$
- $x_1 = 960 \times 0.5 = 480, \; y_1 = 540 \times 0.5 = 270$
- $tw = 1920/2 = 960, \; th = 1080/2 = 540$

El resultado: la región central de $960 \times 540$ se estira a $1920 \times 1080$ → zoom 2x centrado en el cursor.

---

## 3. Interpolación (Linear vs. Nearest Neighbor)

El filtrado de la textura al escalar determina la calidad:
- `BILINEAR` (`WLR_SCALE_FILTER_BILINEAR`): Interpolación suave entre píxeles. Recomendado para texto y UI.
- `NEAREST` (`WLR_SCALE_FILTER_NEAREST`): Sin interpolación. Para precisión pixel-perfect en diseño/programación.

---

## 4. Animación del Factor de Zoom
El cambio de factor no es instantáneo, sino interpolado suavemente con una **animación de progresión simple**:

$$f(t) = lerp(f_{start}, f_{target}, ease(t))$$

El hook de renderizado se desactiva automáticamente cuando $|f - 1| \leq 0.01$ (zoom prácticamente en 1x).

---

## 5. Implementación en C++20 / Vulkan

En nuestro motor:
1. El zoom es un **post-process Vulkan** que opera sobre el `VkImage` del framebuffer final.
2. Se usa `vkCmdBlitImage` con filtro `VK_FILTER_LINEAR` o `VK_FILTER_NEAREST` para copiar la región recortada al framebuffer de presentación.
3. El factor y las coordenadas de la región se actualizan cada frame como `Push Constants`.
# Análisis Matemático: Wall/VSwipe (Transición Plana 2D entre Workspaces)

## Fuentes de Referencia
- `wayfire/plugins/single_plugins/vswipe.cpp`
- `wayfire/plugins/single_plugins/vswipe-processing.hpp`
- `compiz-0.9.14.2/plugins/wall/src/wall.cpp`

---

## 1. Modelo de Workspace Wall

El **Wall** (pared de escritorios) representa todos los workspaces como un mosaico 2D infinito. La transición entre workspaces consiste en un **deslizamiento del viewport** sobre este mosaico.

```
  Workspace (0,0)  │  Workspace (1,0)  │  Workspace (2,0)
───────────────────┼───────────────────┼──────────────────
  Workspace (0,1)  │  [ACTIVO]         │  Workspace (2,1)
───────────────────┼───────────────────┼──────────────────
  Workspace (0,2)  │  Workspace (1,2)  │  Workspace (2,2)
```

---

## 2. Interpolación del Viewport en la Transición

La animación se maneja como una **interpolación lineal** entre el rectángulo viewport del workspace actual y el del workspace destino.

### A. Rectángulo de Workspace
Para el workspace $(vx, vy)$ con dimensiones $W \times H$ y gap $g$ entre workspaces:
$$R_{ws}(vx, vy) = \{x = (W+g) \cdot vx,\; y = (H+g) \cdot vy,\; w = W,\; h = H\}$$

### B. Interpolación Temporal
El viewport animado interpola entre los rectángulos del workspace actual y el siguiente:

$$viewport(t) = lerp(R_{ws}(vx, vy),\; R_{ws}(vx + dx, vy + dy),\; \alpha(t))$$

Componente a componente ($\alpha_x$ y $\alpha_y$ pueden ser diferentes en movimiento diagonal):
$$viewport.x(t) = (1 - \alpha_x) \cdot R_1.x + \alpha_x \cdot R_2.x$$
$$viewport.y(t) = (1 - \alpha_y) \cdot R_1.y + \alpha_y \cdot R_2.y$$

En la implementación, `smooth_delta.dx` y `smooth_delta.dy` representan el desplazamiento acumulado normalizado $\alpha \in [0, 1]$ por componente.

---

## 3. Procesamiento de Delta del Gesto (Rubber-Band Resistance)

El algoritmo de vswipe incorpora un efecto de resistencia elástica (*rubber-band*) cuando se intenta deslizar más allá del workspace límite:

$$ease(offset) = 1 - |offset|^4 - 0.025$$

$$slowdown = \text{clamp}(ease, s_{min}, 1.0)$$

Donde $s_{min}$ depende de la dirección del movimiento:
- Continuando hacia el límite: $s_{min} = 0.005$ (casi detenido).
- Volviendo desde el límite: $s_{min} = 0.2$ (respuesta más ágil).

**Delta procesado** (con límite de velocidad `speed_cap`):
$$\delta_{proc} = \text{clamp}(\delta_{raw}, -speed\_cap, speed\_cap) \cdot slowdown$$

---

## 4. Criterio de Finalización (Snap Decision)

Al soltar el gesto, se determina si el workspace cambia basándose en:
1. **Umbral de posición** (`move_threshold`, típico 35%): Si el desplazamiento supera el 35% del ancho del workspace → cambiar.
2. **Umbral de velocidad** (`fast_threshold`): Si el último delta de velocidad supera el umbral → cambiar aunque no se haya alcanzado el umbral de posición.

$$target\_dx = \begin{cases} \lfloor dx_{acc} \rfloor + 1 & \text{si } (dx_{acc} - \lfloor dx_{acc} \rfloor > threshold) \lor (v > fast\_threshold) \\ \lfloor dx_{acc} \rfloor & \text{en caso contrario} \end{cases}$$

---

## 5. Implementación en C++20 / Vulkan

En nuestro motor:
1. Cada workspace se renderiza en un **framebuffer Vulkan independiente** (`VkFramebuffer`).
2. El viewport de la pared se implementa como un **Scissor Rect dinámico** de Vulkan (`VkRect2D`) combinado con una traslación afín en el Vertex Shader.
3. Los offsets $\alpha_x, \alpha_y$ se pasan como **Push Constants** actualizados cada frame.
# Análisis Matemático: Fade & Animaciones Generales de Ventana (Aparecer/Desaparecer)

## Fuentes de Referencia
- `wayfire/plugins/animate/animate.cpp`
- `wayfire/plugins/animate/basic_animations.hpp`
- `wayfire/plugins/animate/zap.hpp`
- `wayfire/plugins/animate/fire/fire.cpp`
- `compiz-0.9.14.2/plugins/fade/src/fade.cpp`
- `compiz-0.9.14.2/plugins/animation/src/animation.cpp`

---

## 1. Marco General de Animaciones de Ventana

Todas las animaciones de apertura/cierre de ventana comparten la misma estructura de ciclo de vida:
- **APPEAR**: Animación cuando una ventana aparece (`map`).
- **CLOSE (HIDE)**: Animación cuando una ventana se cierra o minimiza (`unmap`).

Cada efecto implementa la interfaz `animation_base_t`:
```cpp
void init(wayfire_view view, duration_t dur, animation_type type);
bool step(); // retorna true mientras la animación está corriendo
void reverse();
```

---

## 2. Fade (Fundido de Opacidad)

El más simple. Interpola el canal alpha de 0 a 1 (o 1 a 0 al cerrar).

$$\alpha(t) = lerp(0, 1, ease(t))$$

Implementado como una `simple_animation_t` (transición temporizad, curva de suavizado configurable).

**En el motor Vulkan**: Multiplicar el valor alpha en el Fragment Shader: `fragColor.a *= alpha_value`.

---

## 3. Zoom-Scale (Aparecer desde el Punto de Activación)

Combina 4 atributos animados simultáneamente con la misma duración:

| Atributo | Inicio (APPEAR) | Fin (APPEAR) |
| :--- | :--- | :--- |
| `alpha` | $0.0$ | $1.0$ |
| `zoom` (escala uniforme) | $1/3$ | $1.0$ |
| `offset_x` | $hint\_cx - view\_cx$ | $0$ |
| `offset_y` | $hint\_cy - view\_cy$ | $0$ |

Donde $(hint\_cx, hint\_cy)$ es el punto de origen de apertura (ej. posición del icono en el dock).

La matriz de transformación en cada frame:
$$M(t) = T(offset\_x(t), offset\_y(t)) \cdot S(zoom(t), zoom(t))$$

---

## 4. Zap (Desintegración Eléctrica)

El efecto zap simula una descarga eléctrica usando curvas de Bézier cuadráticas deformadas aleatoriamente.

### A. Generación del Rayo
La posición del rayo en cada segmento $(x_i, y_i)$ se calcula con ruido aditivo:
$$x_i = lerp(x_{start}, x_{end}, t_i) + \delta_x^{(i)}$$

Donde $\delta_x^{(i)} \sim U(-amplitude, amplitude)$ es un desplazamiento aleatorio por segmento.

### B. Decaimiento de Amplitud
La amplitud de la perturbación disminuye hacia el final de la animación:
$$amplitude(t) = A_{max} \cdot (1 - progress(t))$$

---

## 5. Interpolación Temporal: Curvas de Suavizado (Easing)

Todos los efectos usan `timed_transition_t` que implementa una de las curvas clásicas de interpolación:

$$ease_{smooth}(t) = 3t^2 - 2t^3 \quad \text{(Smoothstep)}$$

$$ease_{cubic}(t) = t^3 \quad \text{(Ease-in cúbico)}$$

$$ease_{elastic}(t) = 1 - e^{-5t}\cos(10\pi t) \quad \text{(Rebote elástico)}$$

La duración se mide en milisegundos y el progreso $t \in [0, 1]$ se calcula como:

$$t = \text{clamp}\left(\frac{elapsed\_ms}{duration\_ms}, 0, 1\right)$$

---

## 6. Tabla Comparativa de Efectos de Animación

| Plugin | Técnica | Parámetros Clave |
| :--- | :--- | :--- |
| **Fade** | Interpolación de alpha | `duration_ms`, curva de easing |
| **Zoom** | Scale + Translate + Alpha | `zoom_start` (1/3), `hint_cx/cy` |
| **Fire** | Sistema de partículas | `fire_particles`, `fire_particle_size` |
| **Zap** | Rayo Bézier deformado | `zap_segments`, `amplitude` |
| **Spin** | Rotación Z + Alpha | `spin_duration`, `rotation_angle` |

---

## 7. Implementación en C++20 / Vulkan

En nuestro motor:
1. Las animaciones de opacidad se implementan con un **Uniform Buffer Object (UBO)** de alpha.
2. Las animaciones de escala y traslación usan **Push Constants** por frame.
3. El sistema de partículas (Fire/Zap) corre en un **Compute Shader** de Vulkan.
4. La función de easing se evalúa en CPU para minimizar el trabajo del shader.
# Análisis Matemático: Grid (Snapping Adaptativo de Ventanas a Zonas)

## Fuentes de Referencia
- `wayfire/plugins/grid/grid.cpp`
- `compiz-0.9.14.2/plugins/grid/src/grid.cpp`

---

## 1. Concepto de Zonas de Snapping

El plugin **Grid** divide el área de trabajo (workarea) en una cuadrícula lógica de zonas, y asigna la geometría de la ventana a la zona más cercana cuando el usuario la arrastra a su borde.

Las 9 zonas estándar corresponden a las posiciones del teclado numérico:

```
 ┌──────────┬──────────┬──────────┐
 │  7 (TL)  │  8 (TC)  │  9 (TR)  │   Top
 ├──────────┼──────────┼──────────┤
 │  4 (ML)  │  5 (MAX) │  6 (MR)  │   Middle
 ├──────────┼──────────┼──────────┤
 │  1 (BL)  │  2 (BC)  │  3 (BR)  │   Bottom
 └──────────┴──────────┴──────────┘
  Left       Center     Right
```

---

## 2. Mapa de Anchos de Slot

Compiz Grid define proporciones de slot basadas en fracciones del `workarea.width`:

| Fracción | Nombre | Píxeles (en W=1920) |
| :--- | :--- | :--- |
| $W / 8$ | slotWidth12 | 240 |
| $W / 6$ | slotWidth17 | 320 |
| $W / 4$ | slotWidth25 | 480 |
| $W / 3$ | slotWidth33 | 640 |
| $W / 2$ | slotWidth50 | 960 |
| $W - W/3$ | slotWidth66 | 1280 |
| $W - W/4$ | slotWidth75 | 1440 |
| $W$ | slotWidth100 | 1920 |

---

## 3. Algoritmo de Selección de Slot

Para cada zona del teclado numérico, se calcula el rectángulo destino:

**Zona 4 (Mitad Izquierda):**
$$slot = \{x = workarea.x, \; y = workarea.y, \; w = W/2, \; h = H\}$$

**Zona 6 (Mitad Derecha):**
$$slot = \{x = workarea.x + W/2, \; y = workarea.y, \; w = W/2, \; h = H\}$$

**Zona 7 (Cuarto Superior Izquierdo):**
$$slot = \{x = workarea.x, \; y = workarea.y, \; w = W/2, \; h = H/2\}$$

**Zona 5 (Máximizado):**
$$slot = \{x = workarea.x, \; y = workarea.y, \; w = W, \; h = H\}$$

### Corrección por Decoraciones de Ventana
El rectángulo efectivo de la ventana se ajusta por los bordes de la decoración:
$$rect_{actual} = \{x + border.left, \; y + border.top, \; w - border.left - border.right, \; h - border.top - border.bottom\}$$

---

## 4. Animación de Snapping (Interpolación Animada)

Cuando una ventana hace snap a una zona, la transición de su geometría se anima suavemente:

$$geometry(t) = lerp(geometry_{current}, geometry_{target}, ease(t))$$

Donde $ease(t)$ es tipicamente una curva smooth-step:
$$ease(t) = 6t^5 - 15t^4 + 10t^3 \quad \text{(Smoother Step)}$$

La geometría original de la ventana se guarda en `originalSize` para poder revertir el snap.

---

## 5. Detección de Zona por Posición del Cursor

En Wayfire (plugin `tile`/`grid`), el snap se detecta basándose en la posición relativa del cursor dentro de la pantalla, dividiendo el espacio en regiones de borde:

- **Borde izquierdo** ($x < threshold$): Snap a zona izquierda.
- **Borde superior** ($y < threshold$): Snap a zona superior.
- **Borde superior izquierdo** ($x < threshold \land y < threshold$): Snap a cuarto superior izquierdo.

El umbral `threshold` es configurable (típicamente $10-20$px del borde).

---

## 6. Implementación en C++20 / Vulkan

En nuestro motor:
1. El snapping es pura **aritmética de rectángulos en CPU** (sin GPU).
2. La animación de la geometría de ventana usa **`std::lerp` de C++20** para interpolar `{x, y, width, height}` frame a frame.
3. El resultado se aplica como una **transformación de escala y traslación** en el Vertex Shader de Vulkan:

```glsl
layout(push_constant) uniform GridTransform {
    float scale_x, scale_y;
    float translate_x, translate_y;
} grid;
```
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
# Análisis Matemático y Modernización: Plugin Freewins (Transformaciones Libres 3D & Direct Input Raycasting)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/freewins/src/freewins.cpp`
- `compiz-0.9.14.2/plugins/freewins/src/freewins.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `freewins` permite la transformación tridimensional arbitraria de la textura de cualquier ventana (rotación en ejes X/Y/Z, escalado asimétrico y cizallamiento/skew), junto con la re-proyección de eventos de entrada.

### A. Matriz de Transformación Homogénea 4x4
La transformación completa de la ventana se representa mediante el producto acumulativo de matrices afines de 4 dimensiones:

$$\mathbf{M}_{view} = \mathbf{T}(dx, dy, dz) \cdot \mathbf{R}_z(\gamma) \cdot \mathbf{R}_y(\beta) \cdot \mathbf{R}_x(\alpha) \cdot \mathbf{H}(h_x, h_y) \cdot \mathbf{S}(s_x, s_y, s_z)$$

Donde:
1. **Traslación**:
   $$\mathbf{T}(dx, dy, dz) = \begin{pmatrix} 1 & 0 & 0 & dx \\ 0 & 1 & 0 & dy \\ 0 & 0 & 1 & dz \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

2. **Rotación Euler en 3 Ejes**:
   $$\mathbf{R}_x(\alpha) = \begin{pmatrix} 1 & 0 & 0 & 0 \\ 0 & \cos\alpha & -\sin\alpha & 0 \\ 0 & \sin\alpha & \cos\alpha & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}, \quad \mathbf{R}_y(\beta) = \begin{pmatrix} \cos\beta & 0 & \sin\beta & 0 \\ 0 & 1 & 0 & 0 \\ -\sin\beta & 0 & \cos\beta & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

3. **Cizallamiento / Skew Matrix**:
   $$\mathbf{H}(h_x, h_y) = \begin{pmatrix} 1 & \tan(h_x) & 0 & 0 \\ \tan(h_y) & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

4. **Escalado Asimétrico desde el Centro**:
   $$\mathbf{S}(s_x, s_y, s_z) = \begin{pmatrix} s_x & 0 & 0 & 0 \\ 0 & s_y & 0 & 0 \\ 0 & 0 & s_z & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

### B. Inversión de Matriz para Redirección de Input en X11
Para que una ventana inclinada o rotada responda al puntero del ratón en Compiz 0.9, el plugin calculaba la **Matriz Inversa** $\mathbf{M}^{-1}$:

$$\mathbf{P}_{local} = \mathbf{M}^{-1} \cdot \mathbf{P}_{screen}$$

Si $x_{local} \in [0, W_{win}]$ y $y_{local} \in [0, H_{win}]$, se sintetizaba un evento de entrada $XSendEvent$.

---

## 2. Ruta de Modernización para C++20 y Vulkan / Wayland

En X11, la redirección de entrada en ventanas 3D era inestable debido al protocolo monolítico del X Server. En **Wayland (GNOME Shell / C++20 Engine)**, la arquitectura de entrada es limpia y basada en raycasting.

### A. Raycasting de Intersección Ray-Plane en 3D
Cuando el usuario interactúa con una ventana rotada en 3D en Wayland:

1. **Unproject del Cursor desde Coordenadas NDC**:
   Sea el cursor en pantalla $(x_{scr}, y_{scr})$, lo convertimos a Normalized Device Coordinates (NDC) $P_{ndc} \in [-1, 1]^3$:
   $$\mathbf{P}_{near} = \mathbf{M}_{proj}^{-1} \cdot \mathbf{M}_{view}^{-1} \cdot \begin{pmatrix} x_{ndc} \\ y_{ndc} \\ -1 \\ 1 \end{pmatrix}$$
   $$\mathbf{P}_{far} = \mathbf{M}_{proj}^{-1} \cdot \mathbf{M}_{view}^{-1} \cdot \begin{pmatrix} x_{ndc} \\ y_{ndc} \\ 1 \\ 1 \end{pmatrix}$$

2. **Rayo 3D del Puntero**:
   $$\mathbf{O}_{ray} = \mathbf{P}_{near}$$
   $$\mathbf{D}_{ray} = \text{normalize}(\mathbf{P}_{far} - \mathbf{P}_{near})$$

3. **Intersección Plano-Rayo con el Plano de la Ventana**:
   Sea el plano de la ventana definido por su centro $\mathbf{P}_0$ y su vector normal transformado $\vec{N}_{win} = \mathbf{M}_{rot} \cdot (0, 0, 1)^T$:
   $$t = \frac{(\mathbf{P}_0 - \mathbf{O}_{ray}) \cdot \vec{N}_{win}}{\mathbf{D}_{ray} \cdot \vec{N}_{win}}$$
   $$\mathbf{P}_{hit} = \mathbf{O}_{ray} + t \cdot \mathbf{D}_{ray}$$

4. **Transformación a Coordenadas Superficie Wayland**:
   $$\begin{pmatrix} u \\ v \end{pmatrix} = \mathbf{M}_{local\_2d}^{-1} \cdot \mathbf{P}_{hit}$$
   Se transmite el evento $wl\_pointer.motion(u, v)$ directamente al cliente Wayland de la aplicación sin latencia.

### B. Rendimiento Vulkan (`Push Constants` + GLM Matrix Math)
- En C++20, se utiliza `glm::mat4` pre-calculada en CPU.
- La matriz final $MVP = \mathbf{P} \cdot \mathbf{V} \cdot \mathbf{M}$ se pasa al Vertex Shader de Vulkan mediante **Push Constants** (16 floats, 64 bytes), evitando asignaciones de memoria secundaria en GPU.

---

## 3. Registro en CaveMem y Verificación

- Se añade el patrón de Raycasting Inverso Wayland a la memoria de arquitectura de `compiz-gnome`.
# Análisis Matemático y Modernización: Plugin Ring (Carrusel Circular / Elíptico 3D)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/ring/src/ring.cpp`
- `compiz-0.9.14.2/plugins/ring/src/ring.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `ring` distribuye las miniaturas de las ventanas abiertas en una trayectoria elíptica o circular 3D alrededor del centro de la pantalla.

### A. Ecuaciones Paramétricas de la Elipse de Navegación
Dada una lista de $N$ ventanas, el índice de ventana $i \in [0, N-1]$ se mapea a un ángulo $\theta_i$:

$$\theta_i = \theta_{base} - i \cdot \left( \frac{2\pi}{N} \right)$$

Donde $\theta_{base} = \frac{2\pi \cdot \text{mRotTarget}}{3600}$ representa el ángulo de rotación global del carrusel.

Las coordenadas de la pantalla $(x_i, y_i)$ de la ventana $i$ se obtienen mediante la parametrización de la elipse:

$$x_i = C_x \pm A \cdot \sin(\theta_i)$$
$$y_i = C_y + B \cdot \cos(\theta_i)$$

Donde:
- $(C_x, C_y) = \left( \frac{W_{screen}}{2}, \frac{H_{screen}}{2} \right)$ es el centro de la pantalla.
- $A = \frac{W_{screen} \cdot \text{RingWidth}}{200}$ es el semieje mayor horizontal.
- $B = \frac{H_{screen} \cdot \text{RingHeight}}{200}$ es el semieje menor vertical.

### B. Interpolación Lineal de Profundidad ($s_{depth}$ y $b_{depth}$)
En Compiz 0.9 (2D compositing), la ilusión de profundidad 3D se lograba interpolando linealmente la escala y el brillo en función de la coordenada $y_i$ (las ventanas con mayor $y$ están más cerca de la pantalla):

$$s_{depth}(y_i) = \text{minScale} + \left( \frac{1.0 - \text{minScale}}{(C_y + B) - (C_y - B)} \right) \cdot (y_i - (C_y - B))$$

$$b_{depth}(y_i) = \text{minBrightness} + \left( \frac{1.0 - \text{minBrightness}}{(C_y + B) - (C_y - B)} \right) \cdot (y_i - (C_y - B))$$

El tamaño final de la miniatura $s_{total}$ combina el escalado para ajustar al bounding box con la profundidad:

$$s_{fit} = \min\left( \frac{\text{thumbWidth}}{W_{win}}, \frac{\text{thumbHeight}}{H_{win}}, 1.0 \right)$$
$$s_{total} = s_{fit} \cdot s_{depth}(y_i)$$

### C. Ordenamiento de Dibujo (Painter's Algorithm)
Para evitar fallos de superposición sin Z-buffer de profundidad, las ventanas se ordenan por su posición $y$:

$$\text{compareRingWindowDepth}(a, b): a.y < b.y \implies a \text{ se dibuja primero (atrás)}$$

---

## 2. Ruta de Modernización para C++20 y Vulkan

En el motor `compiz-gnome` C++20/Vulkan:

### A. Espacio 3D Real con Cámara de Perspectiva (True 3D Orbit)
Sustituimos el escalado 2D simulado por un verdadero **anillo 3D en el plano XZ**:

$$\mathbf{P}_i = \begin{pmatrix} A \cdot \sin(\theta_i) \\ 0 \\ B \cdot \cos(\theta_i) \end{pmatrix}$$

Matriz de Transformación por Ventana:
$$\mathbf{M}_i = \mathbf{T}(\mathbf{P}_i) \cdot \mathbf{R}_y(-\theta_i) \cdot \mathbf{S}(s_{fit})$$

La matriz de proyección en perspectiva $\mathbf{P}_{cam} \cdot \mathbf{V}_{cam}$ aplica automáticamente el acortamiento de perspectiva ($1/z$), el ordenamiento de profundidad por **Hardware Z-Buffer** (`VkPipelineDepthStencilStateCreateInfo`), y la atenuación de luz focal.

### B. Inercia y Suavizado Amortiguado (Spring-Damper Dynamics)
En C++20, la rotación de rueda/teclado utiliza integración numérica RK4 o Spring-Damper amortiguado crítico:

$$a(t) = -k (\theta - \theta_{target}) - c \cdot v(t)$$
$$v(t + \Delta t) = v(t) + a(t) \Delta t$$
$$\theta(t + \Delta t) = \theta(t) + v(t + \Delta t) \Delta t$$

---

## 3. Registro en CaveMem

- Registrada la modernización de Ring Switcher a 3D real XZ orbit con Vulkan Depth Buffer.
# Análisis Matemático y Modernización: Plugin Shift (Cover Flow & Flip Switcher con Reflexión Planar)

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/shift/src/shift.cpp`
- `compiz-0.9.14.2/plugins/shift/src/shift.h`

---

## 1. Deconstrucción Matemática del Código Fuente Original (Compiz 0.9)

El plugin `shift` implementa dos modos de navegación 3D de ventanas: **ModeCover** (estilo Apple Cover Flow) y **ModeFlip** (estilo apilamiento lineal en ángulo), ambos con soporte de **Reflexión Planar en el Suelo**.

---

### A. Modo Cover Flow (`layoutThumbsCover`)

Para cada ventana $i$ con distancia flotante al foco $d = \text{mMvTarget} - i$:

#### 1. Zona Central ($|d| < 1.0$)
La ventana transiciona suavemente hacia o desde el centro del escenario usando una proyección sinusoidal de $90^\circ$:

$$\text{pos} = \sin\left(d \cdot \frac{\pi}{2}\right)$$
$$x(d) = C_x + \text{pos} \cdot \text{space} \cdot \text{extraSpace}$$
$$z(d) = -|d| \cdot \frac{\text{maxThumbWidth}}{2 \cdot W_{screen}}$$
$$\theta_{rot}(d) = -\text{pos} \cdot \theta_{cover}$$

Donde $\theta_{cover} \approx 65^\circ$ es el ángulo de inclinación lateral del abanico.

#### 2. Zona Lateral ($|d| \ge 1.0$)
Las ventanas laterales se disponen en un arco logarítmico con ángulo decreciente para evitar solapamientos excesivos:

$$\text{ang}(d) = \left(\frac{\pi}{\max(72, 2N)}\right) \cdot (d - \text{pos}) + \text{pos} \cdot \left(\frac{\pi}{6}\right)$$
$$x(d) = C_x + \sin(\text{ang}) \cdot \text{rad} \cdot W_{screen} \cdot \text{extraSpace}$$
$$\theta_{rot}(d) = -\text{pos} \cdot \left( \theta_{cover} + 30^\circ - |\text{ang}| \cdot \frac{180^\circ}{\pi} \right)$$
$$z(d) = -\left(\frac{\text{maxThumbWidth}}{2 W_{screen}}\right) - \cos\left(\frac{\pi}{6}\right) \cdot \text{rad} + \cos(\text{ang}) \cdot \text{rad}$$

---

### B. Modo Flip (`layoutThumbsFlip`)

Las ventanas forman una fila en diagonal inclinada con ángulo constante $\phi_{flip}$:

$$x(d) = C_x + \sin(\phi_{flip}) \cdot d \cdot \left(\frac{\text{maxThumbWidth}}{2}\right)$$
$$z(d) = \cos(\phi_{flip}) \cdot d \cdot \left(\frac{\text{maxThumbWidth}}{2 W_{screen}}\right)$$
$$\text{opacity}(d) = \max(0.0, 1.0 - d)$$

---

### C. Reflexión Planar sobre el Suelo (Floor Mirroring)
En Compiz 0.9, el reflejo en el suelo se lograba dibujando la ventana dos veces: la segunda vez con escalado vertical invertido $S_y = -1$:

$$\mathbf{M}_{reflect} = \mathbf{T}(x, y_{floor}, z) \cdot \mathbf{S}(s_x, -s_y, 1) \cdot \mathbf{R}_y(\theta)$$
$$\alpha_{reflect} = \alpha_{win} \cdot \text{reflectBrightness}$$

---

## 2. Ruta de Modernización para C++20 y Vulkan

En `compiz-gnome` C++20/Vulkan:

### A. Matriz de Reflexión Planar Homogénea 4x4 Exacta
Sustituimos la inversión ad-hoc $S_y = -1$ por la matriz de reflexión planar formal contra el plano del suelo $\Pi = (0, 1, 0, -y_{floor})^T$:

$$\mathbf{R}_{plane} = \mathbf{I} - 2 \mathbf{N} \mathbf{N}^T = \begin{pmatrix} 1 & 0 & 0 & 0 \\ 0 & -1 & 0 & 2 y_{floor} \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$

Matriz final del reflejo:
$$\mathbf{M}_{reflect} = \mathbf{R}_{plane} \cdot \mathbf{M}_{window}$$

### B. Desenfoque de Suelo Glossy / Matte con Vulkan Compute Shader
En lugar de un reflejo de espejo perfecto y plano:
1. Se renderiza la reflexión en una textura temporal.
2. Un **Compute Shader de Desenfoque Gaussiano 1D** aplica un blur dependiente de la distancia vertical al suelo:
   $$\sigma(y) = \sigma_{base} + k \cdot (y_{floor} - y)$$
3. El resultado genera un suelo de mármol/cristal esmerilado (*glossy floor*) de calidad cinematográfica.

---

## 3. Registro en CaveMem

- Registrada la matriz de reflexión planar $\mathbf{R}_{plane}$ y el blur de suelo Glossy con Compute Shader en CaveMem.
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
# Análisis Matemático y Modernización: Los 7 Efectos Visuales Secundarios de Compiz

## Fuentes de Referencia
- `compiz-0.9.14.2/plugins/firepaint/src/firepaint.cpp`
- `compiz-0.9.14.2/plugins/cubeaddon/src/cubeaddon.cpp`
- `compiz-0.9.14.2/plugins/mblur/src/mblur.cpp`
- `compiz-0.9.14.2/plugins/reflex/src/reflex.cpp`
- `compiz-0.9.14.2/plugins/colorfilter/src/colorfilter.cpp`

---

## 1. Firepaint (Pintado Interactivo de Fuego)

### A. Física de Partículas Emitidas por Cursor
Cuando el usuario arrastra el cursor $(x, y)$, emite partículas en una cola de emisión. Para cada partícula $p$:

$$x(t+\Delta t) = x(t) + \frac{v_x}{slowdown}, \quad y(t+\Delta t) = y(t) + \frac{v_y}{slowdown}$$
$$v_x(t+\Delta t) = v_x(t) + a_x \cdot dt, \quad v_y(t+\Delta t) = v_y(t) + a_y \cdot dt$$

Donde la aceleración ascendente $a_y = -3.0$ (convección térmica de llama) y la vida $life(t) \leftarrow life(t) - fade \cdot dt$.

### B. Modernización Vulkan: Particle Compute Shader
- En Vulkan C++20, en vez de almacenar arreglos en CPU y mapear `GLVertexBuffer`, se usa un **Vulkan Storage Buffer (SSBO)** de $N=100,000$ partículas actualizadas por un `VkComputePipeline` de 1 pase y renderizadas con `vkCmdDrawIndirect`.

---

## 2. Motion Blur (`mblur` - Desenfoque de Movimiento Acumulativo)

### A. Integración Temporal Exponencial (Accumulation Buffer)
`mblur` genera un rastro de movimiento reteniendo los frames anteriores en una textura de acumulación:

$$F_{accum}(t) = \alpha \cdot F_{curr}(t) + (1 - \alpha) \cdot F_{accum}(t - \Delta t)$$

Donde $\alpha \in (0, 1]$ es el factor de persistencia.

### B. Modernización Vulkan
- En Vulkan C++20, se utiliza un `VkImage` intermedio de alta precisión (`VK_FORMAT_R16G16B16A16_SFLOAT`) mezclado dinámicamente durante el pase de composición.

---

## 3. CubeAddon (Cubo Esférico, Reflejo en Suelo y Cap Images)

### A. Morphing Geométrico Cubo $\to$ Esfera
El plugin intercala linealmente entre la superficie del cubo 3D y la esfera parametrizada:

$$\mathbf{P}_{sphere} = R \cdot \text{normalize}(\mathbf{P}_{cube})$$
$$\mathbf{P}_{final}(t) = (1 - \lambda) \cdot \mathbf{P}_{cube} + \lambda \cdot \mathbf{P}_{sphere}, \quad \lambda \in [0, 1]$$

### B. Reflexiones de Cielo/Suelo (Skybox & Ground Reflection)
Renderiza el reflejo sobre el plano $Y=0$ con atenuación de Fresnel $F(\theta)$ y textura de cielo desvanecida en el horizonte.

---

## 4. 3D Windows (`td` - Ventanas con Inclinación Depth-of-Field)

Aplica una transformación de rotación 3D inclinada $\mathbf{R}_x(\theta)$ a las ventanas no enfocadas y un desenfoque Gaussiano proporcional a la distancia $Z$ a la cámara:

$$\text{blur\_radius}(z) = k \cdot |z - z_{focus}|$$

---

## 5. Filtros de Color (`obs`, `colorfilter`, `neg`)

Transformaciones matriciales $4 \times 4$ en espacio de color aplicadas en el Fragment Shader:

$$\begin{pmatrix} R' \\ G' \\ B' \\ A' \end{pmatrix} = \mathbf{M}_{color} \cdot \begin{pmatrix} R \\ G \\ B \\ A \end{pmatrix}$$

- **Inversión (Negativo)**: $\mathbf{M}_{neg} = \begin{pmatrix} -1 & 0 & 0 & 1 \\ 0 & -1 & 0 & 1 \\ 0 & 0 & -1 & 1 \\ 0 & 0 & 0 & 1 \end{pmatrix}$
- **Daltonismo (Deuteranopia/Protanopia)**: Multiplicación por la matriz de corrección LMS de LMS-to-RGB.

---

## 6. Reflexión Especular (`reflex`)

Aplica una textura de mapa de reflejo espejado $\mathbf{T}_{reflex}(u, v)$ con degradado alpha en el borde superior o inferior de las ventanas:

$$C_{out} = C_{win} + \alpha_{reflex}(v) \cdot C_{gloss}$$
