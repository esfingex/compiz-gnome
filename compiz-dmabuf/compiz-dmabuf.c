// compiz-dmabuf/compiz-dmabuf.c
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "compiz-dmabuf.h"

static GType compiz_dmabuf_export_result_get_type_once(void) {
    return g_boxed_type_register_static("CompizDmabufExportResult",
                                        (GBoxedCopyFunc) compiz_dmabuf_export_result_copy,
                                        (GBoxedFreeFunc) compiz_dmabuf_export_result_free);
}

GType compiz_dmabuf_export_result_get_type(void) {
    static GType type = 0;
    if (g_once_init_enter(&type)) {
        GType t = compiz_dmabuf_export_result_get_type_once();
        g_once_init_leave(&type, t);
    }
    return type;
}

CompizDmabufExportResult *compiz_dmabuf_export_result_copy(CompizDmabufExportResult *src) {
    if (!src) return NULL;
    CompizDmabufExportResult *dst = g_new(CompizDmabufExportResult, 1);
    *dst = *src;
    if (src->fd >= 0) dst->fd = dup(src->fd);
    return dst;
}

void compiz_dmabuf_export_result_free(CompizDmabufExportResult *res) {
    if (!res) return;
    if (res->fd >= 0) close(res->fd);
    g_free(res);
}

static PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR = NULL;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = NULL;
static PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR = NULL;
static PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR = NULL;
static PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR = NULL;
static PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID = NULL;

static void load_egl_extensions(EGLDisplay dpy) {
    (void)dpy;
    if (eglCreateImageKHR) return;
    eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    eglWaitSyncKHR = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
    eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
    eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
    eglDupNativeFenceFDANDROID = (PFNEGLDUPNATIVEFENCEFDANDROIDPROC)eglGetProcAddress("eglDupNativeFenceFDANDROID");
}

CompizDmabufExportResult *compiz_dmabuf_export_window_texture(gpointer window_actor) {
    (void)window_actor;
    CompizDmabufExportResult *res = g_new0(CompizDmabufExportResult, 1);
    res->fd = -1;
    res->success = FALSE;
    return res;
}

EGLImageKHR compiz_dmabuf_import_to_egl_image(EGLDisplay display, int fd, uint32_t width, uint32_t height,
                                              uint32_t stride, uint32_t format, uint64_t modifier) {
    load_egl_extensions(display);
    if (!eglCreateImageKHR) return EGL_NO_IMAGE_KHR;

    EGLint attrs[] = {
        EGL_WIDTH, (EGLint)width,
        EGL_HEIGHT, (EGLint)height,
        EGL_LINUX_DRM_FOURCC_EXT, (EGLint)format,
        EGL_DMA_BUF_PLANE0_FD_EXT, fd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, (EGLint)stride,
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(modifier & 0xFFFFFFFF),
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(modifier >> 32),
        EGL_NONE
    };

    EGLImageKHR img = eglCreateImageKHR(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attrs);
    if (img == EGL_NO_IMAGE_KHR) {
        g_warning("[compiz-dmabuf] eglCreateImageKHR falló: 0x%x", eglGetError());
    }
    return img;
}

gboolean compiz_dmabuf_wait_sync_fd(int fd, uint64_t timeout_ns) {
    (void)timeout_ns;
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    load_egl_extensions(dpy);
    if (!eglWaitSyncKHR || !eglCreateSyncKHR) return FALSE;

    EGLint attrs[] = { EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fd, EGL_NONE };
    EGLSyncKHR sync = eglCreateSyncKHR(dpy, EGL_SYNC_NATIVE_FENCE_ANDROID, attrs);
    if (sync == EGL_NO_SYNC_KHR) return FALSE;

    EGLint result = eglWaitSyncKHR(dpy, sync, 0);
    eglDestroySyncKHR(dpy, sync);
    return (result == EGL_TRUE);
}

int compiz_dmabuf_create_release_sync_fd(EGLDisplay display) {
    if (display == EGL_NO_DISPLAY) {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }
    load_egl_extensions(display);
    if (!eglCreateSyncKHR || !eglDupNativeFenceFDANDROID) return -1;

    EGLSyncKHR sync = eglCreateSyncKHR(display, EGL_SYNC_NATIVE_FENCE_ANDROID, NULL);
    if (sync == EGL_NO_SYNC_KHR) return -1;

    int fd = eglDupNativeFenceFDANDROID(display, sync);
    eglDestroySyncKHR(display, sync);

    if (fd < 0) {
        g_warning("[compiz-dmabuf] Error al dup sync fd: %s", strerror(errno));
    }
    return fd;
}
