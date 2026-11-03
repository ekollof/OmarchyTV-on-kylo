/* Host-only software EGL test adapter: preserve the production display1
 * request contract, but route the test to Mesa's default surfaceless display. */
#include <EGL/egl.h>
#include <assert.h>
#include <stdint.h>
#include <GLES2/gl2.h>
extern EGLDisplay __real_eglGetDisplay(EGLNativeDisplayType native);
EGLDisplay __wrap_eglGetDisplay(EGLNativeDisplayType native)
{
    assert((uintptr_t)native==1);
    return __real_eglGetDisplay(EGL_DEFAULT_DISPLAY);
}
int oa_glyph_fail_program,oa_glyph_fail_texture;
extern GLuint __real_glCreateProgram(void);
GLuint __wrap_glCreateProgram(void)
{
    if(oa_glyph_fail_program){oa_glyph_fail_program=0;return 0;}
    return __real_glCreateProgram();
}
extern void __real_glGenTextures(GLsizei count,GLuint *textures);
void __wrap_glGenTextures(GLsizei count,GLuint *textures)
{
    if(oa_glyph_fail_texture) {
        oa_glyph_fail_texture=0;
        for(GLsizei i=0;i<count;++i)textures[i]=0;
        return;
    }
    __real_glGenTextures(count,textures);
}
