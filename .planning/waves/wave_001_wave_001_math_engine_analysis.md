# Wave 001 — Math Engine & Universal Architecture Analysis

## Wave Objective
Realizar la deconstrucción matemática de los plugins emblemáticos (Wobbly Windows, 3D Cube y Burn/Efecto Fuego) comparando Compiz 0.9.14.2 y Wayfire, y diseñar la arquitectura del motor universal C++20 / Vulkan con aceleración por hardware y comunicación no invasiva (DMA-BUF / IPC / Shaders) para GNOME Shell y otros entornos.

## Requirements & Acceptance Criteria
- [ ] Req 1: Analizar la física de resorte-amortiguador (spring-damper mesh grid) de Wobbly Windows en Compiz y Wayfire, extrayendo las ecuaciones diferenciales discretizadas.
- [ ] Req 2: Analizar la proyección 3D, rotaciones de la esfera/cubo y skybox de 3D Cube en Compiz y Wayfire.
- [ ] Req 3: Analizar la generación de partículas, ruido Perlin/Simplex y deformación de geometría para el efecto de quemar ventanas (Burn effect).
- [ ] Req 4: Diseñar el blueprint del motor universal desacoplado en C++20/Vulkan con la especificación de Shaders y del protocolo de aceleración multiplataforma para GNOME Shell (Mutter).

## Tasks List (Mapped to Requirements)
- [ ] Task 1 (Req 1): Crear `docs/math_wobbly_analysis.md` comparando el código de física de `compiz-0.9.14.2/plugins/wobbly/` y `wayfire/plugins/wobbly/`.
- [ ] Task 2 (Req 2): Crear `docs/math_cube_analysis.md` comparando las transformaciones 3D de `compiz-0.9.14.2/plugins/cube/` y `wayfire/plugins/cube/`.
- [ ] Task 3 (Req 3): Crear `docs/math_burn_analysis.md` analizando los algoritmos de partículas y fuego de `compiz-0.9.14.2/plugins/animation/` (efecto burn).
- [ ] Task 4 (Req 4): Documentar la especificación de la arquitectura del motor universal C++20/Vulkan y la capa de extensión no invasiva en `docs/universal_engine_architecture.md`.

## Verification Plan
- [ ] Verification 1: Confirmar la consistencia matemática entre las ecuaciones extraídas y el código fuente de referencia.
- [ ] Verification 2: Ejecutar `python3 /home/esfingex/workspace/cachy-ai-tools/scripts/forge.py wave-check` para validar la alineación de la Wave.

## Alignment Q&A (Interaction Notes)
- **Q1**: Para el motor C++20/Vulkan universal, ¿prefieres que implementemos primero la simulación física de Wobbly Windows o la rotación 3D del Cubo de Escritorio?
- **A1**: Analizar e integrar los 3 efectos principales: Wobbly Windows, 3D Desktop Cube y Burn (Efecto de quemar ventanas).
- **Q2**: Para la comunicación no invasiva entre el motor C++20/Vulkan y GNOME Shell, ¿cuál arquitectura prefieres?
- **A2**: Arquitectura basada en C++20 y Vulkan por su portabilidad multiplataforma universal (compatible con cualquier GPU integrada/dedicada en laptops o PCs de escritorio) exportando texturas/shaders sin importar la versión del sistema operativo.
