#include <glib.h>
#include <glib-object.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <stdio.h>

// Helper GObject C nativo para GJS (Introspección GObject)
// Proporciona exportación/importación EGL DMA-BUF y eglWaitSyncKHR directa

G_BEGIN_DECLS

#define COMPIZ_TYPE_DMABUF (compiz_dmabuf_get_type())
G_DECLARE_FINAL_TYPE(CompizDmabuf, compiz_dmabuf, COMPIZ, DMABUF, GObject)

struct _CompizDmabuf {
    GObject parent_instance;
};

G_DEFINE_TYPE(CompizDmabuf, compiz_dmabuf, G_TYPE_OBJECT)

static void compiz_dmabuf_class_init(CompizDmabufClass* klass) {
    (void)klass;
}

static void compiz_dmabuf_init(CompizDmabuf* self) {
    (void)self;
}

// Ejecuta eglWaitSyncKHR pasándole el sync_fd importado desde Vulkan por IPC
gboolean compiz_egl_wait_sync_fd(int sync_fd) {
    EGLDisplay display = eglGetCurrentDisplay();
    if (display == EGL_NO_DISPLAY) {
        g_warning("[compiz-dmabuf] EGLDisplay no válido.");
        return FALSE;
    }

    PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR =
        (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR =
        (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglGetProcAddress");

    if (!eglCreateSyncKHR || !eglWaitSyncKHR) {
        g_warning("[compiz-dmabuf] Extensión EGL_KHR_fence_sync no soportada.");
        return FALSE;
    }

    EGLint attribs[] = {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID, sync_fd,
        EGL_NONE
    };

    EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_NATIVE_FENCE_ANDROID, attribs);
    if (sync == EGL_NO_SYNC_KHR) {
        g_warning("[compiz-dmabuf] Error al crear EGLSyncKHR desde sync_fd %d", sync_fd);
        return FALSE;
    }

    EGLint status = eglWaitSyncKHR(display, sync, 0);
    return (status == EGL_TRUE);
}

G_END_DECLS
