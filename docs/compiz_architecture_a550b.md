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
- **Deformación Geométrica (Mesh Grids)**: Plugins como `Wobbly` (ventanas gelatinosas) subdividían la textura de la ventana en una malla 2D de vértices y aplicaban una simulación física de masa-resorte (*spring-damper*).
- **Proyección 3D (Scene Graph / Skybox)**: Plugins como `Cube` proyectaban los escritorios virtuales (*viewports*) sobre las caras de un prisma 3D o esfera.

---

## 2. Mapeo a GNOME Moderno (Wayland + Mutter)

En GNOME 40+ y 50+, la arquitectura cambia drásticamente:

| Componente | Compiz (X11 Clásico) | GNOME Moderno (Wayland) |
| :--- | :--- | :--- |
| **Display Server** | X.Org Server (X11) | **Mutter** (Servidor Wayland integrado) |
| **Compositor** | Compiz (Proceso independiente) | **Mutter / GNOME Shell** (Proceso único) |
| **Texturas de Ventana** | XComposite + GLX | Subsuperficies Wayland (`wl_surface` / `dma-buf`) |
| **Scene Graph** | Custom OpenGL matrix stack | **Clutter** (Cogl / GSK / GTK4 Rendering Engine) |
| **API de Extensiones** | Plugins C/C++ nativos (DL open / VTable override) | **GJS** (JavaScript / SpiderMonkey) + Shaders GLSL |

---

## 3. Estrategia de Migración y Desarrollo

Para lograr el nivel de flexibilidad de Compiz en GNOME moderno sin modificar los fuentes de Mutter:

1. **Capa GJS / Clutter (Extensión GNOME Shell)**:
   - Intercepta actores de ventanas en GNOME Shell (`global.get_window_actors()`).
   - Aplica efectos de transformación (`ClutterEffect`) y Shaders GLSL personalizados.

2. **Capa C++ / Vulkan (Motor de Física y Render Offscreen)**:
   - Realiza cálculos intensivos en C++20 (ej. mallas deformables de ventanas, física de fluidos, cálculo de matrices 3D).
   - Exporta resultados mediante IPC / Shared Memory / `dma-buf` para que GNOME Shell los dibuje a 60+ FPS sin saturar el hilo de JavaScript (GJS).

---

## 4. Comparativa: Compiz 0.9 vs. Wayfire vs. GNOME Shell / Mutter

| Criterio | Compiz 0.9.14.2 (X11) | Wayfire (Wayland) | Nuestro Proyecto (Compiz-GNOME) |
| :--- | :--- | :--- | :--- |
| **Rol en el sistema** | Composite Manager para X11 | Servidor Wayland autónomo (wlroots) | Extensión + Engine C++/Vulkan para GNOME Shell |
| **Base de Código** | C++03 / C++11 (Wrapper sobre C X11) | C++17 / C++20 moderno (wlroots) | C++20 + GJS / Shaders GLSL |
| **Arquitectura de Plugins** | `CompPlugin` vtables dinámicas | `wf::plugin_interface_t` + señales C++ | ClutterEffect / Shaders + Helper C++ |
| **Compatibilidad GNOME** | Obsolescente (Requiere sesión X11) | Incompatible (Es otro compositor rival de Mutter) | **Nativa para GNOME Shell 50+ (Wayland)** |
| **Utilidad de Referencia** | Código original de referencia histórica | **Referencia matemática limpia en C++17/20** | Objetivo final ejecutable en GNOME Shell |

