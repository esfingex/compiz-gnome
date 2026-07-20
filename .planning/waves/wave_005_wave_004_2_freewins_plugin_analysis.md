# Wave 005 — wave_004_2_freewins_plugin_analysis

## Wave Objective
Deconstrucción matemática del plugin Freewins de Compiz 0.9.14.2 (transformaciones libres 3D: rotación, escala, skew) y especificación de modernización para Vulkan / Wayland (Raycasting Ray-Plane Unproject).

## Requirements & Acceptance Criteria
- [x] Req 1: Analizar las matrices de transformación homogéneas 4x4 (traslación, rotación Euler 3D, skew/cizallamiento, escala) del código fuente de Compiz (`freewins.cpp`).
- [x] Req 2: Diseñar el pipeline C++20/Vulkan con Push Constants (GLM mat4) e intersección de rayos 3D en Wayland para redirección de input precisa.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Analizar `compiz-0.9.14.2/plugins/freewins/src/freewins.cpp`.
- [x] Task 2 (Req 2): Crear `docs/math_freewins_analysis.md` con la matemática completa y la arquitectura Raycasting Wayland.

## Verification Plan
- [x] Verification 1: Confirmar las ecuaciones de transformación 3D, cizallamiento y la geometría Ray-Plane NDC.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿Cómo se solucionará la redirección de entrada de ventanas 3D rotadas en Wayland?
- **A**: Con Raycasting 3D exacto des-proyectando el puntero del ratón desde el frustum NDC hasta el plano transformado de la ventana.
