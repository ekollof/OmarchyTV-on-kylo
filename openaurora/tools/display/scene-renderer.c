/* Native existing-fb1 renderer. EGL-WINDOW records the ARM-verified profile,
 * DMA export and swap/MAU behavior. Loading Mali has process-local effects;
 * the first explicit graphics/device operation here is the OPEN callback. */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "openaurora/scene_renderer.h"
#include "openaurora/scene_gles.h"
static int open_scene(void **owner, unsigned width, unsigned height)
{
    const char *framebuffer = getenv("FRAMEBUFFER_2"), *mau_env = getenv("MALI_MAU_SWITCH");
    if (!framebuffer || strcmp(framebuffer, "/dev/fb1") ||
        !mau_env || strcmp(mau_env, "0,0,0,0,0")) { errno = EINVAL; return -1; }
    int fd = open("/dev/fb1", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    unsigned mau = 1;
    int failed = ioctl(fd, FBIOGET_VSCREENINFO, &v) || ioctl(fd, FBIOGET_FSCREENINFO, &f) ||
                 ioctl(fd, 0x8004c8bdu, &mau);
    int primary = errno;
    if (close(fd) && !failed) { failed = 1; primary = errno; }
    if (failed) { errno = primary; return -1; }
    if (v.xres != width || v.yres != height || v.xres_virtual != width ||
        v.yres_virtual != height*2 || v.bits_per_pixel != 32 || v.xoffset ||
        (v.yoffset != 0 && v.yoffset != height) || f.line_length != width*4 ||
        f.smem_len != width*height*8 || mau != 0) { errno = ENOTSUP; return -1; }
    oa_scene_gles *scene = *owner;
    int result = oa_scene_gles_open_window(&scene, width, height);
    *owner = scene;
    return result;
}
static int render(void *owner, const uint8_t *rgba, size_t size, unsigned type, float progress)
{ return oa_scene_gles_render(owner, rgba, size, (int)type, progress, NULL, 0); }
static int finish(void *owner) { return oa_scene_gles_finish(owner); }
static int close_scene(void **owner)
{
    oa_scene_gles *scene = *owner;
    int result = oa_scene_gles_close(&scene);
    *owner = scene;
    return result;
}
const oa_scene_renderer oa_scene_renderer_v1 = {
    OA_SCENE_RENDERER_ABI, sizeof(oa_scene_renderer), {open_scene, render, finish, close_scene}
};
