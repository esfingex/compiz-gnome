# Wave 008 — wave_004_5_animation_plugin_analysis

## Wave Objective
Deconstrucción matemática de la suite completa de animaciones de Compiz 0.9.14.2 (Magic Lamp, Curl, Explode, Roll Up, Dodge) y especificación de modernización para Vulkan (Tessellation Shaders & Mesh Shaders 1.3).

## Requirements & Acceptance Criteria
- [x] Req 1: Analizar la curva sigmoide de Magic Lamp, las curvas Bézier de Curl, la teselación de polígonos de Explode y el enrollado cilíndrico de Roll Up del código fuente (`magiclamp.cpp`, `curvedfold.cpp`, etc.).
- [x] Req 2: Diseñar la arquitectura C++20/Vulkan con Vulkan Tessellation Evaluation Shaders y Mesh Shaders para ejecución 100% GPU en paralelo.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Analizar `compiz-0.9.14.2/plugins/animation/src/magiclamp.cpp` y archivos relacionados.
- [x] Task 2 (Req 2): Crear `docs/math_animation_full_analysis.md` con las ecuaciones y la especificación Vulkan Tessellation/Mesh Shader.

## Verification Plan
- [x] Verification 1: Confirmar las ecuaciones sigmoide $S(f_x)$, Bézier $\mathbf{B}(t)$ y cilíndrica $y_{cyl}, z_{cyl}$.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿Se usará Tessellation Shader en Vulkan para Magic Lamp?
- **A**: Sí, lo que permite subdivisión adaptativa sin recalcular arreglos de vértices en CPU.
