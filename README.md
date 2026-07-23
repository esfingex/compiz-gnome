# 🌀 compiz-gnome

<p align="center">
  <img src="https://img.shields.io/badge/Vulkan-1.3-red.svg?style=for-the-badge&logo=vulkan" alt="Vulkan 1.3">
  <img src="https://img.shields.io/badge/C++-20-blue.svg?style=for-the-badge&logo=cplusplus" alt="C++20">
  <img src="https://img.shields.io/badge/GNOME--Shell-50+-blueviolet.svg?style=for-the-badge&logo=gnome" alt="GNOME Shell 50+">
  <img src="https://img.shields.io/badge/License-GPL--3.0-green.svg?style=for-the-badge" alt="GPL-3.0">
  <img src="https://img.shields.io/badge/IPC-FlatBuffers%20Zero--Copy-orange.svg?style=for-the-badge" alt="FlatBuffers">
</p>

**compiz-gnome** es un port moderno y de alto rendimiento de los emblemáticos **88 plugins de efectos de Compiz (2006-2012)** para el entorno de escritorio **GNOME Shell 50+ en Wayland**.

Combina un **motor C++20 / Vulkan 1.3 Offscreen (Headless)** desacoplado a **120Hz fijos**, comunicación **IPC Zero-Copy vía FlatBuffers y SCM_RIGHTS**, e importación/exportación de texturas VRAM mediante **DMA-BUF con Timeline Semaphores**.

---

## ⚡ Arquitectura del Sistema

```
  ┌────────────────────────────────────────────────────────┐
  │                 GNOME Shell 50+ (Wayland)               │
  │  ┌────────────────────┐      ┌──────────────────────┐  │
  │  │   extension.js     │      │  compiz-dmabuf (C)   │  │
  │  │ Clutter / Mutter   │◄────►│ GObject Introspection│  │
  │  └─────────┬──────────┘      └──────────┬───────────┘  │
  └────────────┼────────────────────────────┼──────────────┘
               │ Unix Socket                │ DMA-BUF / VRAM
               │ (FlatBuffers IPC)          │ (Timeline Semaphores)
  ┌────────────▼────────────────────────────▼──────────────┐
  │         compiz-engine-host (Motor C++20 / Vulkan)      │
  │                                                        │
  │  ┌──────────────────────────────────────────────────┐  │
  │  │             FixedTimestep Loop (120Hz)           │  │
  │  └────────────────────────┬─────────────────────────┘  │
  │                           │                            │
  │  ┌────────────────────────▼─────────────────────────┐  │
  │  │                  FrameGraph GPU                  │  │
  │  │  ┌──────────────┐  ┌──────────────┐  ┌─────────┐  │  │
  │  │  │ WaterCompute │  │ WobblySpring │  │  Kawase │  │  │
  │  │  │     Pass     │  │     Pass     │  │  Blur   │  │  │
  │  │  └──────────────┘  └──────────────┘  └─────────┘  │  │
  │  └──────────────────────────────────────────────────┘  │
  └────────────────────────────────────────────────────────┘
```

---

## 🚀 Características Principales

- 💧 **Water Ripple Drag**: Simulación física de fluidos FDM 2D en Compute Shader a 120Hz con memoria compartida LDS ($18\times 18$), refracción de Snell, aberración cromática RGB y resplandor spec Fresnel.
- 🪟 **Wobbly Windows**: Grilla de resortes GPU con masa-amortiguador, fuerza externa por cursor, viento aleatorio determinístico y renderizado Bézier bicúbica completa (4×4).
- 🔥 **Burn & Firepaint**: Sistema de partículas GPU (65k partículas), hash PCG32, gradiente de plasma por temperatura de 4 colores y seguimiento de cursor.
- 🌀 **Shift Switcher / Cover Flow**: Carrusel de ventanas con reflexión planar espejada, Fresnel-Schlick y blur glossy proporcional a la distancia al suelo.
- 🎯 **Ring Switcher 3D**: Carrusel orbital 3D con tilt, perspectiva por profundidad y rotación de ventanas mirando al centro.
- 🎬 **Animation Suite**: Transiciones de ventanas con teselación GPU adaptativa (Magic Lamp con curva sigmoide, Curl de papel y Explode en fragmentos 3D).
- 🧊 **Blur Dual-Kawase**: Filtrado de desenfoque multicapa hiper-optimizado ($O(8N)$ samples vs $O(R^2)$ de Gauss) con radio dinámico.

---

## 🎨 Cobertura de Plugins

| Categoría | Plugins Soportados / Planificados | Estado Vulkan |
|:---|:---|:---|
| **Física Pesada** | Water, Wobbly, Burn, Firepaint, Animation Suite (Magic Lamp, Curl, Explode) | 🟢 **100% Shaders y Passes Implementados** |
| **Geometría 3D** | Desktop Cube, Rotate, 3D Windows, Ring Switcher, Shift Switcher | 🟢 **100% Shaders y Passes Implementados** |
| **Óptica & Filtros** | Dual-Kawase Blur, Motion Blur, ColorFilter, Reflex, Focus Fade | 🟢 **100% Shaders y Passes Implementados** |
| **Infraestructura** | Grid, Window Rules (Regex), Maximumize, Snap, Annotate, Group | 🟡 **Extensiones GJS en Desarrollo** |

---

## 🛠️ Requisitos de Compilación

### Arch Linux / Manjaro
```bash
sudo pacman -S gcc cmake meson ninja vulkan-headers vulkan-icd-loader \
               glslang egl-wayland glib2 gobject-introspection socat
```

---

## 🔨 Compilación e Instalación

### 1. Compilar el Motor C++20
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```

### 2. Compilar Shaders GLSL a SPIR-V
```bash
mkdir -p build/spv
for shader in src/shaders/*.comp src/shaders/*.frag src/shaders/*.vert src/shaders/*.tesc src/shaders/*.tese; do
    glslangValidator --target-env vulkan1.3 -V "$shader" -o "build/spv/$(basename $shader).spv"
done
```

### 3. Compilar e Instalar el Helper C (`compiz-dmabuf`)
```bash
cd compiz-dmabuf
meson setup build
ninja -C build
sudo ninja -C build install
```

### 4. Instalar la Extensión GNOME Shell
```bash
EXT_DIR="$HOME/.local/share/gnome-shell/extensions/compiz-gnome@esfingex"
mkdir -p "$EXT_DIR"
cp src/gnome_extension/*.js src/gnome_extension/metadata.json "$EXT_DIR/"
gnome-extensions enable compiz-gnome@esfingex
```

---

## 🧪 Pruebas de Integración

Ejecuta el script de pruebas automatizado:

```bash
bash tests/run_integration_tests.sh
```

O consulta la [Guía de Prueba Visual Manual](tests/manual_visual_test.md).

---

## 📜 Licencia

Este proyecto está licenciado bajo la **GNU General Public License v3.0 (GPL-3.0)**. Consulta el archivo [LICENSE](LICENSE) para más detalles.

```
compiz-gnome — Modern Port of Compiz Effects for GNOME Shell 50+
Copyright (C) 2026 esfingex & compiz-gnome Contributors.
```
