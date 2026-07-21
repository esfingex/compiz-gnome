# Wave 011 — wave_005_2_water_physics_compute_pass

## Wave Objective
Implementar el subsistema de simulación física de fluidos FDM 2D en Vulkan Compute Shader a 120Hz fijos, los shaders GLSL de simulación, mapa de normales y refracción de Snell, y el nodo `WaterComputePass` en C++20.

## Requirements & Acceptance Criteria
- [x] Req 1: Implementar el acumulador de tiempo físico `FixedTimestep` a 120Hz fijos ($dt = 8.33\text{ms}$) desacoplado del VSync de presentación.
- [x] Req 2: Crear los Compute Shaders GLSL 460 `water_sim.comp` (con memoria compartida LDS $18\times 18$ `float16_t` e integración Verlet) y `water_normals.comp` (derivadas centrales).
- [x] Req 3: Crear el Fragment Shader `water_effect.frag` (refracción de Snell en espacio de pantalla, aberración cromática RGB y resplandor especular Fresnel-Schlick).
- [x] Req 4: Implementar la clase C++20 `WaterComputePass` con inyección de impacto thread-safe (`std::mutex`) y dispatch de Push Constants.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Crear `src/core_cpp/fixed_timestep.hpp` y `src/core_cpp/wave_fdm_constants.hpp`.
- [x] Task 2 (Req 2): Crear `shaders/water_sim.comp` y `shaders/water_normals.comp`.
- [x] Task 3 (Req 3): Crear `shaders/water_effect.frag`.
- [x] Task 4 (Req 4): Crear `src/core_cpp/passes/water_compute_pass.hpp` y `src/core_cpp/passes/water_compute_pass.cpp`.

## Verification Plan
- [x] Verification 1: Verificar la compiliación C++20 de `WaterComputePass` y la validez de los GLSL Shaders.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿La simulación de agua se desacopla del VSync de la pantalla?
- **A**: Sí, mediante un acumulador `FixedTimestep` a 120Hz fijos, garantizando estabilidad física determinista en pantallas de 60Hz a 240Hz.
