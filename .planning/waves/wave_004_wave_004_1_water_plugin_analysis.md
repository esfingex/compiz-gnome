# Wave 004 — wave_004_1_water_plugin_analysis

## Wave Objective
Deconstrucción matemática y propuesta de optimización Vulkan para el plugin Water de Compiz 0.9.14.2 (simulación de superficie de agua 2D FDM, refracción e iluminación especular).

## Requirements & Acceptance Criteria
- [x] Req 1: Analizar la ecuación diferencial de onda 2D FDM y stencil de 5 puntos del código fuente de Compiz (`water.cpp`, `shaders.h`).
- [x] Req 2: Diseñar la arquitectura de modernización a Vulkan Compute Shaders (`VkComputePipeline`) con memoria compartida local y refracción física de Snell con aberración cromática.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Analizar `compiz-0.9.14.2/plugins/water/src/shaders.h` y `water.cpp`.
- [x] Task 2 (Req 2): Crear `docs/math_water_analysis.md` con las matemáticas y la especificación Vulkan.

## Verification Plan
- [x] Verification 1: Verificar que el documento referencie el código fuente real y contenga las ecuaciones exactas y la propuesta Vulkan Compute.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿El plugin Water se modernizará a Vulkan Compute Shader en lugar de rasterizado FBO clásico?
- **A**: Sí, usando `VkComputePipeline` con memoria local compartida (LDS) y óptica mejorada (Snell + Aberración Cromática RGB).
