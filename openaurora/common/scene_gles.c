#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "openaurora/scene_gles.h"
#include "openaurora/ui_anim.h"

struct oa_scene_gles {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int initialized, bound, ready;
    unsigned width, height;
    size_t bytes;
    pid_t pid;
    pthread_t thread;
    GLuint program, texture;
    GLint opacity;
    const char *operation;
    EGLint egl_error;
    GLenum gl_error;
};
static int fail(oa_scene_gles *s, const char *operation)
{
    s->operation = operation;
    s->egl_error = eglGetError();
    errno = EIO;
    return -1;
}
static int owned(const oa_scene_gles *s)
{
    if (!s) { errno = ENODEV; return -1; }
    if (s->pid != getpid() || !pthread_equal(s->thread, pthread_self())) {
        errno = EPERM; return -1;
    }
    return 0;
}
static int gl_ok(oa_scene_gles *s, const char *operation)
{
    s->gl_error = glGetError();
    return s->gl_error == GL_NO_ERROR ? 0 : fail(s, operation);
}
static GLuint shader(oa_scene_gles *s, GLenum kind, const char *source)
{
    GLuint object = glCreateShader(kind);
    if (!object) { (void)fail(s, "create shader"); return 0; }
    glShaderSource(object, 1, &source, NULL);
    glCompileShader(object);
    GLint compiled = 0;
    glGetShaderiv(object, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(object);
        (void)fail(s, "compile shader");
        return 0;
    }
    return object;
}
int oa_scene_gles_open(oa_scene_gles **out, unsigned width, unsigned height)
{
    if (!out || !width || !height || width > INT_MAX || height > INT_MAX ||
        width > SIZE_MAX / 4 / height) { errno = EINVAL; return -1; }
    if (*out) { errno = EBUSY; return -1; }
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) { errno = EBUSY; return -1; }
    oa_scene_gles *s = calloc(1, sizeof *s);
    if (!s) return -1;
    s->display = EGL_NO_DISPLAY; s->surface = EGL_NO_SURFACE; s->context = EGL_NO_CONTEXT;
    s->width = width; s->height = height; s->bytes = (size_t)width * height * 4;
    s->pid = getpid(); s->thread = pthread_self(); s->operation = "allocate owner";
    *out = s; /* retain partial initialization until close succeeds */
    s->display = eglGetDisplay((EGLNativeDisplayType)(uintptr_t)1);
    if (s->display == EGL_NO_DISPLAY) return fail(s, "get display1");
    if (!eglInitialize(s->display, NULL, NULL)) return fail(s, "initialize display");
    s->initialized = 1;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return fail(s, "bind ES API");
    const EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(s->display, attrs, &config, 1, &count) || count != 1)
        return fail(s, "choose pbuffer config");
    const EGLint dimensions[] = {EGL_WIDTH, (EGLint)width, EGL_HEIGHT, (EGLint)height, EGL_NONE};
    s->surface = eglCreatePbufferSurface(s->display, config, dimensions);
    if (s->surface == EGL_NO_SURFACE) return fail(s, "create pbuffer");
    const EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    s->context = eglCreateContext(s->display, config, EGL_NO_CONTEXT, version);
    if (s->context == EGL_NO_CONTEXT) return fail(s, "create ES2 context");
    if (!eglMakeCurrent(s->display, s->surface, s->surface, s->context))
        return fail(s, "make context current");
    s->bound = 1;
    GLint max_texture = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    if (max_texture <= 0 || width > (unsigned)max_texture || height > (unsigned)max_texture)
        return fail(s, "texture dimensions");
    const char *vertex_source =
        "attribute vec2 position; attribute vec2 uv; varying mediump vec2 coord;"
        "void main(){gl_Position=vec4(position,0.0,1.0);coord=uv;}";
    const char *fragment_source =
        "precision mediump float; varying vec2 coord; uniform sampler2D image;"
        "uniform float opacity; void main(){gl_FragColor=texture2D(image,coord);"
        "gl_FragColor.a*=opacity;}";
    GLuint vertex = shader(s, GL_VERTEX_SHADER, vertex_source);
    if (!vertex) return -1;
    GLuint fragment = shader(s, GL_FRAGMENT_SHADER, fragment_source);
    if (!fragment) { glDeleteShader(vertex); return -1; }
    s->program = glCreateProgram();
    glAttachShader(s->program, vertex); glAttachShader(s->program, fragment);
    glBindAttribLocation(s->program, 0, "position"); glBindAttribLocation(s->program, 1, "uv");
    glLinkProgram(s->program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint linked = 0;
    glGetProgramiv(s->program, GL_LINK_STATUS, &linked);
    if (!linked) return fail(s, "link scene program");
    glUseProgram(s->program);
    s->opacity = glGetUniformLocation(s->program, "opacity");
    GLint image = glGetUniformLocation(s->program, "image");
    if (s->opacity < 0 || image < 0) return fail(s, "scene uniforms");
    glUniform1i(image, 0);
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &s->texture); glBindTexture(GL_TEXTURE_2D, s->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glClearColor(0, 0, 0, 0);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1);
    glPixelStorei(GL_PACK_ALIGNMENT, 4); glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    if (gl_ok(s, "initialize scene state")) return -1;
    s->ready = 1;
    return 0;
}
int oa_scene_gles_render(oa_scene_gles *s, const uint8_t *rgba, size_t input_size,
                         int type, float progress, uint8_t *pixels, size_t output_size)
{
    if (owned(s)) return -1;
    if (!rgba || !pixels || input_size < s->bytes || output_size < s->bytes ||
        type < 0 || type > 8 || !isfinite(progress) || progress < 0 || progress > 1) {
        errno = EINVAL; return -1;
    }
    if (!s->ready) { errno = ENODEV; return -1; }
    if (eglGetCurrentContext() != s->context) { errno = EBUSY; return -1; }
    float x[4] = {-1, 1, 1, -1}, y[4] = {-1, -1, 1, 1};
    oa_fs_apply(type, progress, x, y, 4);
    /* CPU input is top-down; invert texture v, preserving clip-space geometry.
     * glReadPixels below remains bottom-up. No framebuffer format is implied. */
    float vertices[16] = {x[0], y[0], 0, 1, x[1], y[1], 1, 1,
                          x[2], y[2], 1, 0, x[3], y[3], 0, 0};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices + 2);
    glUniform1f(s->opacity, type ? progress : 1.0f);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)s->width, (GLsizei)s->height,
                    GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glReadPixels(0, 0, (GLsizei)s->width, (GLsizei)s->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return gl_ok(s, "render/read scene");
}
int oa_scene_gles_close(oa_scene_gles **out)
{
    if (!out) { errno = EINVAL; return -1; }
    oa_scene_gles *s = *out;
    if (!s) return 0;
    if (owned(s)) return -1;
    EGLContext current = eglGetCurrentContext();
    if (current != EGL_NO_CONTEXT && current != s->context) { errno = EBUSY; return -1; }
    s->ready = 0;
    if (s->bound) {
        if (!eglMakeCurrent(s->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
            return fail(s, "unbind context");
        s->bound = 0;
    }
    /* Destroying this unshared context releases its shaders/textures too. */
    if (s->context != EGL_NO_CONTEXT) {
        if (!eglDestroyContext(s->display, s->context)) return fail(s, "destroy context");
        s->context = EGL_NO_CONTEXT;
    }
    if (s->surface != EGL_NO_SURFACE) {
        if (!eglDestroySurface(s->display, s->surface)) return fail(s, "destroy pbuffer");
        s->surface = EGL_NO_SURFACE;
    }
    if (s->initialized) {
        if (!eglTerminate(s->display)) return fail(s, "terminate display");
        s->initialized = 0;
    }
    if (!eglReleaseThread()) return fail(s, "release EGL thread");
    free(s); *out = NULL;
    return 0;
}
void oa_scene_gles_error(const oa_scene_gles *s, char *out, size_t size)
{
    if (out && size) snprintf(out, size, "%s EGL=0x%x GL=0x%x", s ? s->operation : "no owner",
                              s ? s->egl_error : 0, s ? s->gl_error : 0);
}
