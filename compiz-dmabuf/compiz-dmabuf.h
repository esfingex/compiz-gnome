// compiz-dmabuf/compiz-dmabuf.h
#pragma once

#include <glib.h>
#include <glib-object.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdint.h>

G_BEGIN_DECLS

typedef struct _CompizDmabufExportResult {
    int fd;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
    uint64_t modifier;
    gboolean success;
} CompizDmabufExportResult;

#define COMPIZ_DMABUF_TYPE_EXPORT_RESULT (compiz_dmabuf_export_result_get_type())
GType compiz_dmabuf_export_result_get_type(void);

CompizDmabufExportResult *compiz_dmabuf_export_result_copy(CompizDmabufExportResult *src);
void compiz_dmabuf_export_result_free(CompizDmabufExportResult *res);

/**
 * compiz_dmabuf_export_window_texture:
 * @window_actor: MetaWindowActor (ClutterActor) de la ventana a capturar.
 *
 * Extrae la textura actual de la ventana y la exporta como DMA-BUF.
 *
 * Returns: (transfer full): Boxed struct con FD y metadata. FD=-1 si fallo.
 */
CompizDmabufExportResult *compiz_dmabuf_export_window_texture(gpointer window_actor);

/**
 * compiz_dmabuf_import_to_egl_image:
 * @display: EGLDisplay actual
 * @fd: DMA-BUF FD recibido por IPC
 * @width, @height, @stride, @format, @modifier: Metadata del buffer
 *
 * Crea un EGLImageKHR importable para textura GL/Vulkan.
 * Returns: EGLImageKHR o EGL_NO_IMAGE_KHR.
 */
EGLImageKHR compiz_dmabuf_import_to_egl_image(EGLDisplay display, int fd, uint32_t width, uint32_t height,
                                              uint32_t stride, uint32_t format, uint64_t modifier);

/**
 * compiz_dmabuf_wait_sync_fd:
 * Bloquea el hilo actual hasta que el fence FD señale.
 * Usa eglWaitSyncKHR internamente.
 * @fd: Sync FD recibido por IPC
 * @timeout_ns: Timeout en nanosegundos
 * Returns: TRUE si signaled, FALSE si timeout/error.
 */
gboolean compiz_dmabuf_wait_sync_fd(int fd, uint64_t timeout_ns);

/**
 * compiz_dmabuf_create_release_sync_fd:
 * Crea un Sync FD que se señalizará cuando el compositor termine de leer la textura.
 * Returns: FD válido (>0) o -1 en error.
 */
int compiz_dmabuf_create_release_sync_fd(EGLDisplay display);

G_END_DECLS
