# Wave 013 — wave_006_1_wobbly_blur_effects_and_tests

## Wave Objective
Implementar los efectos Wobbly Windows (simulación de grilla de resortes en GPU Compute + Vertex Shader Bézier) y Blur Dual-Kawase (N passes downsample/upsample), junto con la batería de pruebas de integración automatizadas y la guía de prueba visual manual.

## Requirements & Acceptance Criteria
- [x] Req 1: Implementar `wobbly_spring.comp` (Compute Shader LDS 10x10, Hooke spring-mass, integración Verlet semi-implícita).
- [x] Req 2: Implementar `wobbly_bezier.vert` + `wobbly_bezier.frag` (interpolación bilineal de malla deformada a NDC).
- [x] Req 3: Crear `WobblyComputePass` (C++20, VkXxx, thread-safe, impulsos externos, registro en FrameGraph).
- [x] Req 4: Implementar `kawase_down.comp` (downsample 5-tap) y `kawase_up.frag` (upsample 8-tap diamante).
- [x] Req 5: Crear `KawaseBlurPass` (C++20, N niveles mip chain, radio variable en runtime).
- [x] Req 6: Crear `tests/run_integration_tests.sh` (7 tests automatizados: compilación, IPC, extensión, shaders, C helper).
- [x] Req 7: Crear `tests/manual_visual_test.md` (guía paso a paso para prueba end-to-end en GNOME Shell Wayland).

## Tasks List (Mapped to Requirements)
- [x] Task 1-2 (Req 1-2): Crear shaders `wobbly_spring.comp`, `wobbly_bezier.vert`, `wobbly_bezier.frag`.
- [x] Task 3 (Req 3): Crear `src/core_cpp/passes/wobbly_compute_pass.hpp`.
- [x] Task 4 (Req 4): Crear shaders `kawase_down.comp`, `kawase_up.frag`.
- [x] Task 5 (Req 5): Crear `src/core_cpp/passes/kawase_blur_pass.hpp`.
- [x] Task 6 (Req 6): Crear `tests/run_integration_tests.sh` (chmod +x, ejecutable).
- [x] Task 7 (Req 7): Crear `tests/manual_visual_test.md`.

## Verification Plan
- [x] Verification 1: `glslangValidator --target-env vulkan1.3` en los 8 shaders → 8/8 ✅.
- [x] Verification 2: `gcc -fsyntax-only` en `compiz-dmabuf.c` → 0 errores.
- [x] Verification 3: `g++ -std=c++20 -fsyntax-only` en `main.cpp` → 0 errores.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿Wobbly y Blur comparten el mismo FrameGraph o tienen pases independientes?
- **A**: Cada efecto registra sus propios passes en el FrameGraph global por ventana. El FrameGraph inserta automáticamente los barriers de imagen entre passes. La cadena de render por ventana es: `WobblyComputePass → KawaseBlurPass → WaterComputePass → water_effect.frag`.
