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
└─────────────────────────────────────┬─────────────────────────────────────┘
                                      │ IPC (Unix Domain Socket / Protobuf)
                                      ▼
┌───────────────────────────────────────────────────────────────────────────┐
│              CAPA 2: Motor Headless C++20 / Vulkan (Efectos GPU)           │
│                                                                           │
│   - Wave Equation Compute Shader (Water)                                  │
│   - Mass-Spring Physics & Bézier Mesh (Wobbly)                            │
│   - 3D Elliptical Orbit & Z-Buffer (Ring Switcher)                        │
│   - Planar Reflection & Glossy Floor Shader (Shift Cover Flow)            │
│   - Tessellation & Mesh Shaders 1.3 (Magic Lamp, Curl, Explode)           │
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
| **Animation Suite** | 🔴 GPU (Vulkan) | Tessellation & Mesh 1.3 | Subdivisión dinámica en GPU (Magic Lamp, Curl, Explode, RollUp). |
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
| **Winrules / Regex** | 🟢 CPU (GJS) | JavaScript + Mutter API | Evaluación de expresiones regulares sobre propiedades de ventanas. |
| **Place / Put** | 🟢 CPU (GJS) | JavaScript + Mutter API | Aritmética de posicionamiento inicial y asignación de viewport. |
| **Snap** | 🟢 CPU (GJS) | JavaScript + Mutter API | Detección de colisión de bordes de ventanas en coordenadas de escritorio. |
| **Maximumize** | 🟢 CPU (GJS) | Algoritmo Sweep-Line | Búsqueda en CPU del rectángulo libre más grande sin solapar vecinas. |
| **Group (Pestañas)** | 🟢 CPU (GJS) | JavaScript + Clutter UI | Agrupación de contenedores y barra de pestañas visual sobre la ventana. |
| **Shelf (Estante)** | 🟢 CPU (GJS) | JavaScript + Clutter UI | Reducción de escala de ventana a miniatura flotante fija. |
| **Trailfocus** | 🟢 CPU (GJS) | Cola circular en JS | Seguimiento de orden de foco e interpolación de opacidad. |
| **Annotate** | 🟡 Híbrido (CPU+GPU) | Mouse Events $\to$ Vulkan | CPU captura trazos de dibujo $\to$ Vulkan renderiza líneas con antialiasing. |
| **Showmouse** | 🟡 Híbrido (CPU+GPU) | Mouse Track $\to$ Vulkan | CPU rastrea cursor $\to$ Vulkan emite partículas/anillos concéntricos. |

---

## 3. Protocolo IPC y Exportación DMA-BUF

1. **Gestión de Eventos (CPU GJS Extension)**:
   La extensión GJS intercepta eventos de inicio (ej. usuario presiona Super+Tab, inicia arrastre de ventana, o cambia de workspace) y envía un mensaje IPC ligero al motor C++20 vía Unix Domain Socket.

2. **Renderizado Offscreen (Motor C++20 Vulkan)**:
   El motor Vulkan procesa la física/geometría y renderiza en un `VkImage` offscreen asignado con el flag `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR`.

3. **Intercambio Cero-Copia (Zero-Copy DMA-BUF Export)**:
   El file descriptor (`fd`) de la textura de Vulkan se exporta mediante `dma-buf` a Clutter/Cogl en GNOME Shell. **Cero copias por CPU o RAM**, transferencia directa VRAM-to-VRAM a tasa de refresco del monitor (60/120/144 FPS).
