# Wave 010 — wave_005_1_engine_core_ipc_plumbing

## Wave Objective
Implementar la infraestructura del motor C++20 / Vulkan 1.3 Headless, el servidor IPC Unix Domain Socket con `SCM_RIGHTS`, el esquema FlatBuffers `ipc.fbs` y el módulo helper C `compiz-dmabuf.c` para GJS.

## Requirements & Acceptance Criteria
- [x] Req 1: Crear el sistema de compilación `CMakeLists.txt` C++20 con soporte para Vulkan 1.3, VMA, FlatBuffers y PkgConfig.
- [x] Req 2: Implementar `vulkan_context.hpp/cpp` con inicialización RAII headless, Timeline Semaphores exportables como `sync_fd` y consulta de DRM Format Modifiers.
- [x] Req 3: Implementar `ipc_server.hpp/cpp` con soporte `SCM_RIGHTS` para transferencia bidireccional de FDs entre Vulkan y GNOME Shell.
- [x] Req 4: Implementar el módulo C nativo `compiz-dmabuf.c/h` y `meson.build` para introspección GObject en GJS y sincronización `eglWaitSyncKHR`.

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Crear `CMakeLists.txt` y `proto/water.proto`.
- [x] Task 2 (Req 2): Crear `src/core_cpp/vulkan_context.hpp` y `src/core_cpp/vulkan_context.cpp`.
- [x] Task 3 (Req 3): Crear `src/core_cpp/ipc_server.hpp` y `src/core_cpp/ipc_server.cpp`.
- [x] Task 4 (Req 4): Crear `compiz-dmabuf/compiz-dmabuf.h`, `compiz-dmabuf/compiz-dmabuf.c`, `compiz-dmabuf/meson.build` y `schemas/ipc.fbs`.

## Verification Plan
- [x] Verification 1: Compilar `main.cpp`, `ipc_server.cpp` con `g++ -std=c++20` (0 errores) y `compiz-dmabuf.c` con `gcc` (0 errores).

## Alignment Q&A (Interaction Notes)
- **Q**: ¿El canal IPC utilizará Protobuf o FlatBuffers?
- **A**: FlatBuffers (`schemas/ipc.fbs`) para parsing zero-copy nativo a velocidad de puntero C++ en el motor y soporte optimizado en GJS.
