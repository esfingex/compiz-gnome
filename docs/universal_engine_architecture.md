# Especificación de Arquitectura: Motor Universal C++20 / Vulkan y Capa No Invasiva

## 1. Visión General de la Arquitectura

El proyecto **Compiz-GNOME** adopta una arquitectura desacoplada en dos capas principales:

```
┌──────────────────────────────────────────────────────────────────┐
│                   CAPA 1: MOTOR UNIVERSAL CORE                   │
│                       (C++20 / Vulkan API)                       │
│                                                                  │
│  ┌───────────────────────┐          ┌─────────────────────────┐  │
│  │ Wobbly Physics Engine │          │  3D Cube & Camera Engine│  │
│  └───────────┬───────────┘          └────────────┬────────────┘  │
│              │                                   │               │
│  ┌───────────▼───────────┐          ┌────────────▼────────────┐  │
│  │ Particle Fire Engine  │          │ Vulkan Render Offscreen │  │
│  └───────────┬───────────┘          └────────────┬────────────┘  │
│              │                                   │               │
│              └─────────────────┬─────────────────┘               │
│                                │                                 │
│                    VK_KHR_external_memory_fd                     │
│                        (DMA-BUF Export)                          │
└────────────────────────────────┬─────────────────────────────────┘
                                 │ Sockets IPC / DMA-BUF FDs
                                 ▼
┌──────────────────────────────────────────────────────────────────┐
│             CAPA 2: ADAPTADOR NO INVASIVO GNOME SHELL             │
│                       (Extension GJS / Mutter)                   │
│                                                                  │
│  - Captura eventos de ventana en GNOME Shell (Move, Resize, Close)│
│  - Importa texturas DMA-BUF al Scene Graph de Clutter/Cogl       │
│  - Renderiza los efectos a 60+ FPS sin modificar GNOME nativo    │
└──────────────────────────────────────────────────────────────────┘
```

---

## 2. Componentes del Motor Universal (C++20 / Vulkan)

### A. Módulo de Física (Physics Thread / Compute Shaders)
- **Wobbly Solver**: Simulación de grilla $M \times N$ de masa-resorte usando integración de Euler/Verlet y parches de Bézier.
- **Particle System**: Compute Shader de Vulkan para actualizar la trayectoria, vida y convección de partículas del efecto de fuego (*Burn*).
- **Cube Transformer**: Cálculo de matrices MVP (Model-View-Projection) y transformación espacial 3D.

### B. Módulo de Renderizado Offscreen (Vulkan Engine)
- Renderiza las mallas deformadas y efectos a buffers de memoria GPU externos (`VkImage` creadas con la extensión `VK_KHR_external_memory`).
- Exporta descriptores de archivo `dma-buf` (`vkGetMemoryFdKHR`) para acceso directo entre procesos sin copia de RAM.

---

## 3. Protocolo de Comunicación IPC (Non-Invasive Adapter Protocol)

La extensión GJS de GNOME Shell y el motor C++20 se comunican a través de un **Unix Domain Socket** binario súper ligero:

### Mensajes de Control (Client -> Server)
```cpp
struct EventMessage {
    uint32_t window_id;
    uint32_t event_type; // MOVE, RESIZE, CLOSE, CUBE_ROTATE
    int32_t x, y, width, height;
    float timestamp;
};
```

### Mensajes de Frame (Server -> Client)
```cpp
struct FrameMessage {
    uint32_t window_id;
    int32_t dma_buf_fd;   // Descriptor de archivo DMA-BUF
    uint32_t width;
    uint32_t height;
    uint32_t format;      // DRM_FORMAT_ARGB8888
};
```

---

## 4. Ventajas de Portabilidad Multi-Plataforma

1. **Soporte de Hardware Total**:
   - Vulkan es compatible con GPUs integradas de laptops (Intel HD/Iris, AMD APUs) y GPUs dedicadas (NVIDIA, AMD).
2. **Independencia del Sistema Operativo**:
   - El motor C++20/Vulkan no depende de Gnome, KDE ni X11; puede ejecutarse en Ubuntu, Arch, Fedora, Debian o CachyOS.
3. **Integración No Invasiva**:
   - Si se deshabilita la extensión de GNOME Shell, el escritorio regresa a su estado normal instantáneamente sin necesidad de reiniciar la sesión de usuario.
