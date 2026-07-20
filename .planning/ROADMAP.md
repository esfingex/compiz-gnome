# Project Roadmap: Compiz Gnome Migration

## Phase 1.0: Architectural Analysis & Knowledge Base (COMPLETED)
- [x] Phase 1.1: Document classic Compiz plugin architecture (Core, Window, Screen, Paint lifecycle).
- [x] Phase 1.2: Document modern GNOME Shell / Mutter architecture (Wayland protocols, Clutter scene graph, GJS extensions).
- [x] Phase 1.3: Document Vulkan + DMA-BUF / PipeWire texture sharing mechanism for GNOME extensions.
- [x] Phase 1.4: Full mathematical deconstruction of all 88 Compiz plugins (Wobbly, Cube, Burn, Blur, Scale, Switcher, Rotate, Zoom, Wall, Water, Freewins, Ring, Shift, Animation Suite, Firepaint, etc.).
- [x] Phase 1.5: Technical Design Specification `MASTER_REFERENCE.md` audited & approved with P0/P1 explicit sync (Timeline Semaphores).

## Phase 2.0: Core C++20 / Vulkan Engine (Vertical Slice MVP)
- [ ] Phase 2.1: CMake & C++20 project structure (`src/core_cpp/`) with Vulkan 1.3 RAII, VMA, and Frame Graph.
- [ ] Phase 2.2: Implement `WaterComputePass` (Fixed Timestep 120Hz FDM wave simulation in Compute Shader).
- [ ] Phase 2.3: Implement DMA-BUF (`VK_KHR_external_memory_fd`) + Timeline Semaphore export IPC server.

## Phase 3.0: GNOME Shell Extension & Shaders
- [ ] Phase 3.1: Create GJS extension skeleton (`src/gnome_extension/`) compatible with GNOME 50+.
- [ ] Phase 3.2: Implement GJS ClutterEffect passthrough & IPC client (`SCM_RIGHTS` fd importer).
- [ ] Phase 3.3: End-to-end integration: Drag motion triggers Water Ripple in GNOME Shell Wayland.

## Phase 4.0: Verification & Optimization
- [ ] Phase 4.1: Performance benchmarks (120+ FPS zero-copy window manipulation under Wayland).
- [ ] Phase 4.2: Packaging, installation scripts, and user documentation.
