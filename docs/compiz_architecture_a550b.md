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
