# Wave 007 — wave_004_4_shift_plugin_analysis

## Wave Objective
Deconstrucción matemática del plugin Shift Switcher (Cover Flow & Flip) de Compiz 0.9.14.2 y especificación de modernización para Vulkan (Matriz de Reflexión Planar 4x4 + Glossy Floor Compute Shader).

## Requirements & Acceptance Criteria
- [x] Req 1: Analizar las ecuaciones paramétricas del abanico Cover Flow, Flip stack y la duplicación de reflejo en el suelo del código fuente (`shift.cpp`).
- [x] Req 2: Diseñar la matriz formal de reflexión planar $\mathbf{R}_{plane} = \mathbf{I} - 2\mathbf{N}\mathbf{N}^T$ y el shader de suelo Glossy con $\sigma(y)$ variable en Vulkan.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Analizar `compiz-0.9.14.2/plugins/shift/src/shift.cpp`.
- [x] Task 2 (Req 2): Crear `docs/math_shift_analysis.md` con las ecuaciones y la especificación Vulkan Glossy Floor Reflection.

## Verification Plan
- [x] Verification 1: Confirmar las ecuaciones de posición/rotación sinusoidal, Flip layout y la matriz $\mathbf{R}_{plane}$.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿Se usará un Compute Shader para el reflejo del suelo en Shift Switcher?
- **A**: Sí, para generar un desenfoque Glossy de suelo dinámico según la distancia vertical.
