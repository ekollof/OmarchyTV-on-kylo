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
#include "openaurora/glyph_quad.h"

struct oa_scene_gles {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int initialized, bound, ready, window, uploaded;
    uint16_t native_window[2]; /* ICE_OsdPlaneOpen allocates width/height as four bytes */
    unsigned width, height;
    size_t bytes;
    pid_t pid;
    pthread_t thread;
    GLuint program, texture;
    GLint opacity;
    GLuint glyph_program,glyph_texture,glyph_buffer;
    GLuint text_framebuffer,text_texture,text_program,text_buffer;
    GLint text_opacity;
    GLint glyph_colour,glyph_image;
    unsigned max_texture;
    int glyph_ready;
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
static int open_scene(oa_scene_gles **out, unsigned width, unsigned height, int window)
{
    if (!out || !width || !height || width > INT_MAX || height > INT_MAX ||
        width > SIZE_MAX / 4 / height || (window && (width > UINT16_MAX || height > UINT16_MAX))) {
        errno = EINVAL; return -1;
    }
    if (*out) { errno = EBUSY; return -1; }
    if (eglGetCurrentContext() != EGL_NO_CONTEXT) { errno = EBUSY; return -1; }
    oa_scene_gles *s = calloc(1, sizeof *s);
    if (!s) return -1;
    s->display = EGL_NO_DISPLAY; s->surface = EGL_NO_SURFACE; s->context = EGL_NO_CONTEXT;
    s->width = width; s->height = height; s->bytes = (size_t)width * height * 4;
    s->window = window;
    s->native_window[0] = (uint16_t)width; s->native_window[1] = (uint16_t)height;
    s->pid = getpid(); s->thread = pthread_self(); s->operation = "allocate owner";
    *out = s; /* retain partial initialization until close succeeds */
    s->display = eglGetDisplay((EGLNativeDisplayType)(uintptr_t)1);
    if (s->display == EGL_NO_DISPLAY) return fail(s, "get display1");
    if (!eglInitialize(s->display, NULL, NULL)) return fail(s, "initialize display");
    s->initialized = 1;
    if (!eglBindAPI(EGL_OPENGL_ES_API)) return fail(s, "bind ES API");
    const EGLint attrs[] = {EGL_SURFACE_TYPE, window ? EGL_WINDOW_BIT : EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(s->display, attrs, &config, 1, &count) || count != 1)
        return fail(s, "choose pbuffer config");
    const EGLint dimensions[] = {EGL_WIDTH, (EGLint)width, EGL_HEIGHT, (EGLint)height, EGL_NONE};
    if (window) {
        const EGLint surface_attrs[] = {EGL_NONE};
        s->surface = eglCreateWindowSurface(s->display, config,
            (EGLNativeWindowType)(uintptr_t)s->native_window, surface_attrs);
    } else {
        s->surface = eglCreatePbufferSurface(s->display, config, dimensions);
    }
    if (s->surface == EGL_NO_SURFACE) return fail(s, "create surface");
    EGLint actual_width = 0, actual_height = 0;
    if (!eglQuerySurface(s->display, s->surface, EGL_WIDTH, &actual_width) ||
        !eglQuerySurface(s->display, s->surface, EGL_HEIGHT, &actual_height) ||
        actual_width != (EGLint)width || actual_height != (EGLint)height)
        return fail(s, "surface dimensions");
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
    s->max_texture=(unsigned)max_texture;
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
int oa_scene_gles_open(oa_scene_gles **out, unsigned width, unsigned height)
{ return open_scene(out, width, height, 0); }
int oa_scene_gles_open_window(oa_scene_gles **out, unsigned width, unsigned height)
{ return open_scene(out, width, height, 1); }
static void scene_state(oa_scene_gles *s)
{
    glUseProgram(s->program);glBindBuffer(GL_ARRAY_BUFFER,0);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,s->texture);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
}
static int glyph_init(oa_scene_gles *s)
{
    if(s->glyph_ready)return 0;
    GLenum pending;
    s->gl_error=GL_NO_ERROR;
    const char *vs="attribute vec4 position;attribute vec2 uv;varying vec2 coord;"
                   "void main(){gl_Position=position;coord=uv;}";
    const char *fs="precision mediump float;varying vec2 coord;uniform sampler2D mask;"
                   "uniform ivec4 ink;void main(){gl_FragColor=vec4(float(ink.r)/255.0,"
                   "float(ink.g)/255.0,float(ink.b)/255.0,"
                   "texture2D(mask,coord).a*(float(ink.a)/255.0));}";
    GLuint v=shader(s,GL_VERTEX_SHADER,vs);if(!v)return -1;
    GLuint f=shader(s,GL_FRAGMENT_SHADER,fs);
    if(!f){glDeleteShader(v);return -1;}
    s->glyph_program=glCreateProgram();
    if(!s->glyph_program){glDeleteShader(v);glDeleteShader(f);goto failed;}
    glAttachShader(s->glyph_program,v);glAttachShader(s->glyph_program,f);
    glBindAttribLocation(s->glyph_program,0,"position");
    glBindAttribLocation(s->glyph_program,1,"uv");
    glLinkProgram(s->glyph_program);glDeleteShader(v);glDeleteShader(f);
    GLint linked=0;glGetProgramiv(s->glyph_program,GL_LINK_STATUS,&linked);
    if(!linked)goto failed;
    s->glyph_colour=glGetUniformLocation(s->glyph_program,"ink");
    s->glyph_image=glGetUniformLocation(s->glyph_program,"mask");
    if(s->glyph_colour<0 || s->glyph_image<0)goto failed;
    glGenTextures(1,&s->glyph_texture);glGenBuffers(1,&s->glyph_buffer);
    if(!s->glyph_texture || !s->glyph_buffer || gl_ok(s,"initialize glyph objects"))goto failed;
    s->glyph_ready=1;return 0;
failed:
    pending=glGetError(); /* preserve errors already consumed by gl_ok */
    if(pending!=GL_NO_ERROR)s->gl_error=pending;
    if(s->glyph_texture)glDeleteTextures(1,&s->glyph_texture);
    if(s->glyph_buffer)glDeleteBuffers(1,&s->glyph_buffer);
    if(s->glyph_program)glDeleteProgram(s->glyph_program);
    s->glyph_texture=s->glyph_buffer=s->glyph_program=0;
    return fail(s,"initialize glyph program");
}
static int draw_glyphs(oa_scene_gles *s,const struct oa_scene_glyph *glyphs,size_t count)
{
    int rc=glyph_init(s);
    if(rc){scene_state(s);return rc;}
    glUseProgram(s->glyph_program);glBindBuffer(GL_ARRAY_BUFFER,s->glyph_buffer);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,s->glyph_texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glUniform1i(s->glyph_image,0);
    for(size_t i=0;i<count;++i) {
        const struct oa_scene_glyph *g=glyphs+i;
        struct oa_glyph_quad q;
        int error=oa_glyph_quad_make((int)s->width,(int)s->height,g->x,g->y,g->width,g->height,
                                    g->clipped?&g->clip:NULL,&q);
        if(error==ENOENT || !g->rgba[3])continue;
        if(error){errno=error;rc=-1;break;}
        glTexImage2D(GL_TEXTURE_2D,0,GL_ALPHA,g->width,g->height,0,GL_ALPHA,GL_UNSIGNED_BYTE,g->coverage);
        glBufferData(GL_ARRAY_BUFFER,sizeof q.vertex,q.vertex,GL_STREAM_DRAW);
        glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,6*sizeof(float),0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,6*sizeof(float),(const void *)(uintptr_t)(4*sizeof(float)));
        const GLint ink[4]={g->rgba[0],g->rgba[1],g->rgba[2],g->rgba[3]};
        glUniform4iv(s->glyph_colour,1,ink);glDrawArrays(GL_TRIANGLE_FAN,0,4);
        if(gl_ok(s,"draw glyph")){rc=-1;break;}
    }
    scene_state(s);
    return rc?rc:gl_ok(s,"restore scene state after glyphs");
}
/* Aurora createFrameBufferObject/onRenderCompleted: render the UI into RGBA,
 * then sample it with linear filtering and the same separate blend factors.
 * The second SRC_ALPHA blend intentionally attenuates translucent RGB again. */
static int text_target(oa_scene_gles *s)
{
    if(s->text_framebuffer)return 0;
    GLenum pending;
    s->gl_error=GL_NO_ERROR;
    GLuint v=0,f=0;
    const char *vs="attribute vec4 position;attribute vec2 uv;varying vec2 coord;"
                   "void main(){gl_Position=position;coord=uv;}";
    const char *fs="precision mediump float;varying vec2 coord;uniform sampler2D image;"
                   "uniform float opacity;void main(){gl_FragColor=texture2D(image,coord);"
                   "gl_FragColor.a*=opacity;}";
    v=shader(s,GL_VERTEX_SHADER,vs);if(!v)goto failed;
    f=shader(s,GL_FRAGMENT_SHADER,fs);if(!f)goto failed;
    s->text_program=glCreateProgram();if(!s->text_program)goto failed;
    glAttachShader(s->text_program,v);glAttachShader(s->text_program,f);
    glBindAttribLocation(s->text_program,0,"position");
    glBindAttribLocation(s->text_program,1,"uv");glLinkProgram(s->text_program);
    GLint linked=0;glGetProgramiv(s->text_program,GL_LINK_STATUS,&linked);
    if(!linked)goto failed;
    s->text_opacity=glGetUniformLocation(s->text_program,"opacity");
    GLint image=glGetUniformLocation(s->text_program,"image");
    if(s->text_opacity<0 || image<0)goto failed;
    glUseProgram(s->text_program);glUniform1i(image,0);
    glGenTextures(1,&s->text_texture);
    glGenFramebuffers(1,&s->text_framebuffer);
    glGenBuffers(1,&s->text_buffer);
    if(!s->text_texture || !s->text_framebuffer || !s->text_buffer)goto failed;
    glBindTexture(GL_TEXTURE_2D,s->text_texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,(GLsizei)s->width,(GLsizei)s->height,
                 0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glBindFramebuffer(GL_FRAMEBUFFER,s->text_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,s->text_texture,0);
    GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status!=GL_FRAMEBUFFER_COMPLETE ||
       gl_ok(s,"allocate text framebuffer"))goto failed;
    glDeleteShader(v);glDeleteShader(f);
    glBindFramebuffer(GL_FRAMEBUFFER,0);scene_state(s);return 0;
failed:
    pending=glGetError();
    if(pending!=GL_NO_ERROR)s->gl_error=pending;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(s->text_framebuffer)glDeleteFramebuffers(1,&s->text_framebuffer);
    if(s->text_texture)glDeleteTextures(1,&s->text_texture);
    if(s->text_buffer)glDeleteBuffers(1,&s->text_buffer);
    if(s->text_program)glDeleteProgram(s->text_program);
    if(v)glDeleteShader(v);
    if(f)glDeleteShader(f);
    s->text_framebuffer=s->text_texture=s->text_buffer=s->text_program=0;
    scene_state(s);return fail(s,"allocate text framebuffer");
}
static int render(oa_scene_gles *s, const uint8_t *rgba, size_t input_size,
                   int type, float progress, uint8_t *pixels, size_t output_size,
                   const struct oa_scene_glyph *glyphs,size_t count,int text_pipeline)
{
    if (owned(s)) return -1;
    if ((rgba ? input_size < s->bytes : input_size != 0) ||
        (s->window ? pixels != NULL || output_size != 0 : !pixels || output_size < s->bytes) ||
        type < 0 || type > 8 || !isfinite(progress) || progress < 0 || progress > 1) {
        errno = EINVAL; return -1;
    }
    if (!s->ready) { errno = ENODEV; return -1; }
    if (!rgba && !s->uploaded) { errno = ENODATA; return -1; }
    if(count>4096 || (count && !glyphs)){errno=EINVAL;return -1;}
    size_t total=0;
    for(size_t i=0;i<count;++i) {
        const struct oa_scene_glyph *g=glyphs+i;
        struct oa_glyph_quad q;
        int error=oa_glyph_quad_make((int)s->width,(int)s->height,g->x,g->y,g->width,g->height,
                                    g->clipped?&g->clip:NULL,&q);
        if(error && error!=ENOENT){errno=error;return -1;}
        if((unsigned)g->width>s->max_texture || (unsigned)g->height>s->max_texture ||
           (size_t)g->width>SIZE_MAX/(size_t)g->height){errno=E2BIG;return -1;}
        size_t bytes=(size_t)g->width*(size_t)g->height;
        if(!g->coverage || g->coverage_size<bytes){errno=EINVAL;return -1;}
        if(bytes>64u*1024u*1024u-total){errno=E2BIG;return -1;}
        total+=bytes;
    }
    if (eglGetCurrentContext() != s->context) { errno = EBUSY; return -1; }
    scene_state(s);
    if(text_pipeline && text_target(s))return -1;
    float x[4] = {-1, 1, 1, -1}, y[4] = {-1, -1, 1, 1};
    oa_fs_apply(type, progress, x, y, 4);
    /* CPU input is top-down; invert texture v, preserving clip-space geometry.
     * glReadPixels below remains bottom-up. No framebuffer format is implied. */
    float vertices[16] = {x[0], y[0], 0, 1, x[1], y[1], 1, 1,
                          x[2], y[2], 1, 0, x[3], y[3], 0, 0};
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices + 2);
    glUniform1f(s->opacity, type ? progress : 1.0f);
    if (rgba) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)s->width, (GLsizei)s->height,
                        GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if (gl_ok(s, "upload scene")) return -1;
        s->uploaded = 1;
    }
    if(text_pipeline)glBindFramebuffer(GL_FRAMEBUFFER,s->text_framebuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    if(count && draw_glyphs(s,glyphs,count)) {
        glBindFramebuffer(GL_FRAMEBUFFER,0);return -1;
    }
    if(text_pipeline) {
        /* FBO texture is already bottom-up; unlike the CPU input, do not flip V. */
        const float screen[24]={-1,-1,0,1,0,0, 1,-1,0,1,1,0,
                                1,1,0,1,1,1, -1,1,0,1,0,1};
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        glUseProgram(s->text_program);glBindBuffer(GL_ARRAY_BUFFER,s->text_buffer);
        glBindTexture(GL_TEXTURE_2D,s->text_texture);
        glBufferData(GL_ARRAY_BUFFER,sizeof screen,screen,GL_STREAM_DRAW);
        glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,6*sizeof(float),0);
        glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,6*sizeof(float),(const void *)(uintptr_t)(4*sizeof(float)));
        glUniform1f(s->text_opacity,1);
        glClear(GL_COLOR_BUFFER_BIT);glDrawArrays(GL_TRIANGLE_FAN,0,4);
        scene_state(s);
    }
    if (s->window) {
        if (gl_ok(s, "draw scene")) return -1;
        if (!eglSwapBuffers(s->display, s->surface)) return fail(s, "swap window");
        return 0;
    }
    glReadPixels(0, 0, (GLsizei)s->width, (GLsizei)s->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    return gl_ok(s, "render/read scene");
}
int oa_scene_gles_render(oa_scene_gles *s,const uint8_t *rgba,size_t input_size,
                         int type,float progress,uint8_t *pixels,size_t output_size)
{ return render(s,rgba,input_size,type,progress,pixels,output_size,NULL,0,0); }
int oa_scene_gles_render_glyphs(oa_scene_gles *s,const uint8_t *rgba,size_t input_size,
                               const struct oa_scene_glyph *glyphs,size_t count,
                               uint8_t *pixels,size_t output_size)
{ return render(s,rgba,input_size,0,1,pixels,output_size,glyphs,count,1); }
int oa_scene_gles_finish(oa_scene_gles *s)
{
    if (owned(s)) return -1;
    if (!s->ready) { errno = ENODEV; return -1; }
    if (eglGetCurrentContext() != s->context) { errno = EBUSY; return -1; }
    glFinish();
    return gl_ok(s, "finish scene");
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
