# Wave 012 — wave_005_3_gjs_extension_optics_sync

## Wave Objective
Implementar el cliente IPC en JavaScript para GNOME Shell 50+ (`ipcClient.js`), la captura de gestos de entrada al arrastrar ventanas, la importación/exportación de DMA-BUF y el handshake de sincronización explícita con Timeline Semaphores.

## Requirements & Acceptance Criteria
- [x] Req 1: Crear `src/gnome_extension/ipcClient.js` usando `Gio.SocketClient` asíncrono y decodificación de mensajería IPC.
- [x] Req 2: Crear el esqueleto de la extensión GNOME Shell 50+ en `src/gnome_extension/extension.js` y `metadata.json`.
- [x] Req 3: Vincular el módulo helper C `compiz-dmabuf` con el pipeline de renderizado de Clutter (`ClutterEffect`).

## Tasks List (Mapped to Requirements)
- [x] Task 1 (Req 1): Crear `src/gnome_extension/ipcClient.js`.
- [x] Task 2 (Req 2): Crear `src/gnome_extension/extension.js` y `src/gnome_extension/metadata.json`.
- [x] Task 3 (Req 3): Probar la integración de funciones C `compiz_dmabuf_wait_sync_fd` e `import_to_egl_image`.

## Verification Plan
- [x] Verification 1: Confirmar sintaxis JavaScript limpia sin dependencias no declaradas para GNOME Shell 50+.

## Alignment Q&A (Interaction Notes)
- **Q**: ¿Cómo maneja GJS la sincronización explícita sin APIs expuestas en JS?
- **A**: Utiliza el módulo C nativo `compiz-dmabuf` (cargado vía GObject Introspection) para llamar directamente a `eglWaitSyncKHR` y `eglDupNativeFenceFDANDROID`.
