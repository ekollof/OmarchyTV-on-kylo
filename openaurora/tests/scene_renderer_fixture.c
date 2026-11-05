/* Process/DSO ownership fixture, never hardware acceptance. -init/-fini let us
 * test loader ordering without compiler-specific constructor attributes. */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "openaurora/scene_renderer.h"
static int descriptor = -1, ready, frames;
void scene_fixture_init(void)
{
    const char *fb = getenv("FRAMEBUFFER_2"), *mau = getenv("MALI_MAU_SWITCH");
    ready = fb && !strcmp(fb, "/dev/fb1") && mau && !strcmp(mau, "0,0,0,0,0") &&
        syscall(SYS_fcntl, 128, F_GETFD) == -1 && errno == EBADF;
    descriptor = open("/dev/null", O_RDWR | O_CLOEXEC);
}
void scene_fixture_fini(void) { if (descriptor >= 0) (void)close(descriptor); }
static int open_scene(void **owner, unsigned width, unsigned height)
{
    if (!ready || descriptor < 3 || syscall(SYS_fcntl, descriptor, F_GETFD) < 0 ||
        width != OA_SCENE_WIDTH || height != OA_SCENE_HEIGHT) { errno = EIO; return -1; }
    *owner = &descriptor; return 0;
}
static int render(void *owner, const uint8_t *rgba, size_t size,
                  unsigned x, unsigned y, unsigned w, unsigned h,
                  unsigned type, float progress)
{
    if (owner != &descriptor || syscall(SYS_fcntl, descriptor, F_GETFD) < 0 ||
        type != 4 || progress != 0.5f ||
        (frames == 0 ? !rgba || size != OA_SCENE_IMAGE_BYTES || rgba[0] != 42 ||
                       x || y || w != OA_SCENE_WIDTH || h != OA_SCENE_HEIGHT
                     : rgba || size || x || y || w || h)) {
        errno = EIO; return -1;
    }
    frames++; return 0;
}
static int gate(const char *name)
{
    const char *path = getenv(name); if (!path) return 0;
    int fd = open(path, O_RDONLY | O_CLOEXEC); if (fd < 0) return -1;
    char byte; ssize_t count = read(fd, &byte, 1);
    (void)close(fd); if (count == 1) return 0;
    errno = EIO; return -1;
}
static int finish(void *owner) { (void)owner; return gate("OA_SCENE_FIXTURE_FINISH_GATE"); }
static int close_scene(void **owner)
{ if (gate("OA_SCENE_FIXTURE_CLOSE_GATE")) return -1; *owner = NULL; return 0; }
const oa_scene_renderer oa_scene_renderer_v1 = {
#ifdef BAD_RENDERER_ABI
    999,
#else
    OA_SCENE_RENDERER_ABI,
#endif
    sizeof(oa_scene_renderer), {open_scene, render, finish, close_scene}
};
