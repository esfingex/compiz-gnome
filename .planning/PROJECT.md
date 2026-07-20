# Project Definition: Compiz Gnome (Modern Wayland Migration)

## System Architecture
- Project Name: compiz-gnome
- Target Environment: Linux (Ubuntu / Arch / CachyOS) - GNOME Shell 50+ on Wayland
- Technologies: C++20, Vulkan API, GJS (GNOME JavaScript), Clutter/Cogl Shaders, Wayland Protocols, DMA-BUF / IPC
- Workspace Root: /home/esfingex/workspace/compiz-gnome
- Source Code Reference: /home/esfingex/Github/compiz-0.9.14.2 (Compiz 0.9.14.2 C++ Official Release)
- Modern C++ Wayland Reference: /home/esfingex/Github/wayfire (Wayfire C++17/20 Wayland Compositor)

## Core Goals
- Deconstruct classic Compiz architecture (Core, Plugins, Composite rendering pipeline, Window physics).
- Re-architect visual effects (Wobbly Windows, 3D Desktop Cube, Expo, Alt-Tab Switcher) for modern GNOME Shell on Wayland.
- Implement C++/Vulkan offscreen rendering engine and GJS GNOME Shell Extension integration layer.

## Folder Layout
- `.planning/`: GSD Spec-driven planning system (PROJECT, ROADMAP, STATE, CONSTITUTION, CHECKLIST, continue-here, waves).
- `docs/`: Technical breakdown of Compiz architecture, Wayland/Mutter mechanics, Vulkan rendering pipelines.
- `src/core_cpp/`: C++20 / Vulkan rendering & physics simulation module.
- `src/gnome_extension/`: GNOME Shell extension (GJS + Clutter/Cogl GLSL shaders).
- `tests/`: Automated unit tests and visual verification tests.
