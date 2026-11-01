/* Explicit opt-in MB230 GPU context diagnostic. No native window, swap, fbdev,
 * platform HAL or stock-process hook. Run only after validating the target
 * libmali pbuffer lifecycle; a loader pass alone does not establish that contract.
 */
#include <stdint.h>
#include <stdio.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

int main(void)
{
    int result = 1, initialized = 0, current = 0;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config;
    EGLint count = 0, major = 0, minor = 0;
    const EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_NONE
    };
    const EGLint dimensions[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    const EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    /* ICE_CreateEGLWindow passes native display1, not EGL_DEFAULT_DISPLAY. */
    display = eglGetDisplay((EGLNativeDisplayType)(uintptr_t)1);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor)) goto cleanup;
    initialized = 1;
    printf("EGL %d.%d vendor=%s version=%s\n", major, minor,
           eglQueryString(display, EGL_VENDOR), eglQueryString(display, EGL_VERSION));
    if (!eglBindAPI(EGL_OPENGL_ES_API) ||
        !eglChooseConfig(display, attributes, &config, 1, &count) || count != 1) goto cleanup;
    surface = eglCreatePbufferSurface(display, config, dimensions);
    if (surface == EGL_NO_SURFACE) goto cleanup;
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, surface, surface, context)) goto cleanup;
    current = 1;
    printf("GL vendor=%s renderer=%s version=%s\n", glGetString(GL_VENDOR),
           glGetString(GL_RENDERER), glGetString(GL_VERSION));
    glViewport(0, 0, 64, 64);
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLubyte pixel[4] = {0};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (glGetError() != GL_NO_ERROR) goto cleanup;
    printf("pbuffer RGBA=%u,%u,%u,%u\n", pixel[0], pixel[1], pixel[2], pixel[3]);
    if (pixel[0] < 63 || pixel[0] > 65 || pixel[1] < 127 || pixel[1] > 129 ||
        pixel[2] < 190 || pixel[2] > 192 || pixel[3] != 255) goto cleanup;
    result = 0;
cleanup:
    if (result) fprintf(stderr, "EGL probe failed: EGL error0x%x\n", eglGetError());
    if (current && !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) result = 1;
    if (context != EGL_NO_CONTEXT && !eglDestroyContext(display, context)) result = 1;
    if (surface != EGL_NO_SURFACE && !eglDestroySurface(display, surface)) result = 1;
    if (initialized && !eglTerminate(display)) result = 1;
    if (!eglReleaseThread()) result = 1;
    puts(result ? "GPU context cleanup completed with an error" : "GPU context/readback/cleanup passed");
    return result;
}
