# Project Roadmap: Compiz Gnome Migration

## Phase 1.0: Architectural Analysis & Knowledge Base
- [ ] Phase 1.1: Document classic Compiz plugin architecture (Core, Window, Screen, Paint lifecycle).
- [ ] Phase 1.2: Document modern GNOME Shell / Mutter architecture (Wayland protocols, Clutter scene graph, GJS extensions).
- [ ] Phase 1.3: Document Vulkan + DMA-BUF / PipeWire texture sharing mechanism for GNOME extensions.

## Phase 2.0: Core C++ / Vulkan Physics & Rendering Engine
- [ ] Phase 2.1: Implement C++20 GLFW/Vulkan sandbox for mesh-based window deformations (Spring-Damper grid for Wobbly Windows).
- [ ] Phase 2.2: Implement 3D Desktop Cube geometry, skybox, and rotation math in Vulkan.
- [ ] Phase 2.3: Add IPC / dma-buf export layer for offscreen texture sharing.

## Phase 3.0: GNOME Shell Extension & Shaders
- [ ] Phase 3.1: Create GJS extension skeleton compatible with GNOME 50+.
- [ ] Phase 3.2: Implement GJS ClutterEffect / Cogl GLSL shaders for window effects.
- [ ] Phase 3.3: Integrate C++/Vulkan helper process with GNOME Shell extension.

## Phase 4.0: Verification & Optimization
- [ ] Phase 4.1: Performance benchmarks (60+ FPS window manipulation under Wayland).
- [ ] Phase 4.2: Packaging, installation scripts, and user documentation.
