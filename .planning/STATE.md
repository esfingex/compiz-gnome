# System State & Architecture Decision Records (ADRs)

## Current Status
- Active Phase: Phase 2.0 - Core C++20 / Vulkan Engine (Vertical Slice MVP)
- Active Blockers: None
- Environment: GNOME Shell 50.3 on Wayland (Linux Ubuntu)

## Architecture Decision Records (ADRs)
- ADR-001: Adopt Alicanto GSD workflow (.planning/) and CaveMem database for persistent memory and annotations.
- ADR-002: Dual-layer approach for Compiz migration: GJS/Clutter Shaders for direct GNOME Shell integration + C++/Vulkan engine for high-performance physics/offscreen rendering.
- ADR-003: Memory-Mapped IPC with SCM_RIGHTS (DMA-BUF & Sync FDs) for Zero-Copy VRAM transfer.
- ADR-004: Vulkan 1.3 Timeline Semaphores (`VK_KHR_timeline_semaphore`) for explicit sync with Mutter (`eglWaitSyncKHR`).
- ADR-005: Vertical Slice MVP strategy: First implementation milestone is Water Ripple on window drag.
- ADR-006: Master Architecture Audit approved: Complete CPU/GPU classification split across 88 plugins, Frame Graph transient aliasing map, Protobuf schema (`water.proto`), and 4-week MVP execution plan.
