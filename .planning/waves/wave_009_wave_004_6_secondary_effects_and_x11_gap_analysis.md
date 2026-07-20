# Wave 009 — wave_004_6_secondary_effects_and_x11_gap_analysis

## Wave Objective
Análisis matemático de los 7 efectos secundarios de Compiz 0.9.14.2 (Firepaint, Motion Blur, CubeAddon, 3D Windows, ColorFilter/OBS, Reflex) y análisis comparativo detallado de los 50 plugins de infraestructura vs GNOME Shell / Wayland moderno.

## Requirements & Acceptance Criteria
- [x] Req 1: Analizar la física y matemática de los 7 efectos secundarios del código fuente (`firepaint.cpp`, `cubeaddon.cpp`, `mblur.cpp`, etc.).
- [x] Req 2: Realizar el análisis comparativo (Gap Analysis) de los 50 plugins de infraestructura (Winrules, Maximumize, Annotate, Group, Shelf, Emerald) vs GNOME Shell y diseñar la hoja de ruta para extensiones GJS modulares.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Crear `docs/math_secondary_effects_analysis.md`.
- [x] Task 2 (Req 2): Crear `docs/compiz_vs_gnome_gap_analysis.md`.

## Verification Plan
- [x] Verification 1: Confirmar la precisión matemática de los 7 efectos secundarios y la tabla comparativa de infraestructura GNOME vs Compiz.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿GNOME Shell moderno supera a Compiz en los plugins de infraestructura?
- **A**: No. Compiz era superior en motor de reglas regex (`winrules`), maximización inteligente (`maximumize`), anotación (`annotate`), agrupación (`group`) y decorador (`emerald`). Se adaptarán como extensiones GJS en `compiz-gnome`.
