/* Exercise the actual display module with a fake fbdev, without opening devices.
 * The commit expectations follow NVTGA_OSD_UpdateRegion/FlipRegion and
 * _ker_osd_fb_PanDisplay, not the old display.pan implementation. */
#include <assert.h>
#include <errno.h>
#include <linux/fb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include "openaurora/fb_snapshot.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

const oa_module *oa_module_entry(void);
static const oa_display_iface *display;
static const oa_display_alpha_iface *alpha;
static struct fb_var_screeninfo var;
static unsigned char pixels[1024];
static uint32_t region[8], mau;
static const char *rotate;
static unsigned commits, last_yoffset, syncs, opens, closes;
static int fail_sync, fail_commit, kernel_suspended, fail_unmap, fail_close;
static unsigned height = 2;
static pthread_t main_thread;
static pthread_mutex_t gate_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cv = PTHREAD_COND_INITIALIZER;
static int block_worker, worker_entered, release_worker, join_error, create_error;
int __real_pthread_tryjoin_np(pthread_t, void **);
int __wrap_pthread_tryjoin_np(pthread_t thread, void **value)
{
    if (join_error) { int rc = join_error; join_error = 0; return rc; }
    return __real_pthread_tryjoin_np(thread, value);
}
int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*fn)(void *), void *ud)
{
    if (create_error) { int rc = create_error; create_error = 0; return rc; }
    return __real_pthread_create(thread, attr, fn, ud);
}

int __wrap_open(const char *path, int flags, ...)
{
    (void)flags;
    assert(!strcmp(path, "/dev/fb1"));
    opens++;
    return 123;
}
int __wrap_close(int fd)
{
    assert(fd == 123);closes++;
    if (fail_close) { --fail_close;errno = EIO;return -1; }
    return 0;
}
int __wrap_system(const char *command) { (void)command; assert(!"unexpected system call"); return -1; }
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    assert(!addr && len <= sizeof pixels &&
        (prot == PROT_READ || prot == (PROT_READ | PROT_WRITE)));
    assert(flags == MAP_SHARED && fd == 123 && offset == 0);
    return pixels;
}
int __wrap_munmap(void *addr, size_t len)
{
    assert(addr == pixels && len <= sizeof pixels);
    if (fail_unmap) { --fail_unmap;errno = EIO;return -1; }
    return 0;
}
int __wrap_msync(void *addr, size_t len, int flags)
{
    assert(addr == pixels && len <= sizeof pixels && flags == MS_SYNC);
    syncs++;
    if (fail_sync) { errno = EIO; return -1; }
    return 0;
}
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    assert(fd == 123);
    if (block_worker && !pthread_equal(main_thread, pthread_self())) {
        pthread_mutex_lock(&gate_mu);worker_entered = 1;pthread_cond_broadcast(&gate_cv);
        while (!release_worker) pthread_cond_wait(&gate_cv, &gate_mu);
        pthread_mutex_unlock(&gate_mu);
    }
    va_list args;
    va_start(args, request);
    void *arg = va_arg(args, void *);
    va_end(args);
    switch (request) {
    case FBIOGET_VSCREENINFO: memcpy(arg, &var, sizeof var); return 0;
    case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo *fix = arg;
        memset(fix, 0, sizeof *fix);
        fix->line_length = var.xres * 4;
        fix->smem_len = fix->line_length * var.yres_virtual;
        return 0;
    }
    case 0x8020c86cu: memcpy(arg, region, 32); return 0;
    case 0x4020c86bu: memcpy(region, arg, 32); return 0;
    case OA_FB_MAU_RD: *(uint32_t *)arg = mau; return 0;
    case FBIOPAN_DISPLAY: {
        const struct fb_var_screeninfo *commit = arg;
        /* _ker_osd_fb_PanDisplay rejects fb-private+3c4 before its lock. */
        if (kernel_suspended) { errno = EFAULT; return -1; }
        assert(syncs > commits); /* sync precedes every attempted commit */
        assert(commit->xres == var.xres && commit->yres == var.yres);
        assert(commit->yoffset + var.yres <= var.yres_virtual);
        last_yoffset = commit->yoffset;
        commits++;
        if (fail_commit) { errno = EIO; return -1; }
        var.yoffset = commit->yoffset;
        return 0;
    }
    default: assert(!"unexpected framebuffer ioctl"); return -1;
    }
}
static void log_message(void *priv, int level, const char *fmt, ...)
{ (void)priv; (void)level; (void)fmt; }
static const char *config(void *priv, const char *key, const char *def)
{
    (void)priv;
#ifdef OA_DISPLAY_NO_PLAT
    if (!strcmp(key,"display.drm_init")) return getenv("OA_TEST_BINDING_HAL")?"1":"0";
#endif
    if (!strcmp(key, "display.pan") && rotate) return rotate;
    if (!strcmp(key, "display.mirror")) return "0";
    if (!strcmp(key, "display.restore_snapshot")) return getenv("OA_TEST_RESTORE") ? "1" : "0";
    return def;
}
static int publish(void *priv, const char *name, uint32_t version, const void *iface)
{
    (void)priv;
    if (!strcmp(name, OA_SVC_DISPLAY_ALPHA)) { assert(version == 1); alpha = iface; return 0; }
    assert(!strcmp(name, OA_SVC_DISPLAY) && version == 2);
    display = iface;
    return 0;
}
static int unpublish(void *priv, const char *name)
{
    (void)priv;
    if (!strcmp(name, OA_SVC_DISPLAY_ALPHA)) alpha = NULL;
    else { assert(!strcmp(name, OA_SVC_DISPLAY)); display = NULL; }
    return 0;
}
static void release(void *priv, const char *name) { (void)priv; (void)name; }
static const oa_host_api api = { .size = sizeof(oa_host_api), .abi = OA_ABI_VERSION,
    .log = log_message, .config_get = config, .publish = publish,
    .unpublish = unpublish, .release = release };

static void *start(unsigned frames, unsigned yoffset, const char *rotation)
{
    memset(&var, 0, sizeof var);
    memset(pixels, 0, sizeof pixels);
    memset(region, 0, sizeof region);
    mau = 0;
    var.xres = var.xres_virtual = 8;
    var.yres = height;
    var.yres_virtual = frames * var.yres;
    var.bits_per_pixel = 32;
    var.yoffset = yoffset;
    rotate = rotation;
    commits = syncs = 0;
    fail_sync = fail_commit = 0;
    void *ctx = NULL;
    assert(oa_module_entry()->init(&api, &ctx) == 0);
    assert(oa_module_entry()->start(ctx) == 0 && display);
    return ctx;
}
static void finish(void *ctx)
{
    for (unsigned i = 0;; ++i) {
        int rc = oa_module_entry()->quiesce(ctx);
        if (!rc) break;
        assert(i < 10000);
        const struct timespec pause = {.tv_nsec = 1000000};nanosleep(&pause, NULL);
    }
    oa_module_entry()->stop(ctx);
    oa_module_entry()->destroy(ctx);
    assert(!display && opens == closes);
}
static void cleanup_error(void *ctx, int error)
{
    assert(oa_module_entry()->quiesce(ctx) == -EAGAIN);
    for (unsigned i = 0;; ++i) {
        assert(i < 10000);
        const struct timespec pause = {.tv_nsec = 1000000};nanosleep(&pause, NULL);
        int rc = oa_module_entry()->quiesce(ctx);
        if (rc == error) return;
        assert(rc == -EBUSY);
    }
}
int main(void)
{
    main_thread = pthread_self();
#ifdef OA_DISPLAY_NO_PLAT
    void *rejected=NULL;
    assert(!strcmp(oa_module_entry()->name,"display_fb"));
    assert(!oa_module_entry()->requires[0][0]);
    assert(!oa_module_entry()->init(&api,&rejected));
    assert(!setenv("OA_TEST_BINDING_HAL","1",1));
    assert(oa_module_entry()->start(rejected)==-1 && opens==0);
    oa_module_entry()->destroy(rejected);
    assert(!unsetenv("OA_TEST_BINDING_HAL"));
#endif
    void *ctx = start(1, 0, NULL);
    assert(alpha && !alpha->clear_rgba(36, 38, 40, 153));
    assert(!display->present());
    assert(pixels[0] == 24 && pixels[1] == 23 && pixels[2] == 22 && pixels[3] == 153);
    const unsigned char semi[] = {200, 100, 50, 128};
    assert(!alpha->clear_rgba(0, 0, 0, 0));
    assert(!display->blit(0, 0, 1, 1, semi) && !display->present());
    assert(pixels[0] == 25 && pixels[1] == 50 && pixels[2] == 100 && pixels[3] == 128);
    assert(!display->set_scene_transform(6, 0.5f) && !display->present());
    assert(pixels[0] == 13 && pixels[1] == 25 && pixels[2] == 50 && pixels[3] == 64);
    finish(ctx);
    puts("  ok   translucent canvas reaches premultiplied BGRA scanout at identity and during fade");

    ctx = start(2, 2, NULL); /* default rotation off, second frame current */
    assert(display->clear(0x12, 0x34, 0x56) == 0);
    assert(display->present() == 0 && commits == 1 && last_yoffset == 2);
    /* the offscreen scene is copied into the committed frame at present */
    assert(pixels[64] == 0x56 && pixels[66] == 0x12);
    assert(display->present() == 0 && commits == 2 && last_yoffset == 2);
    assert(oa_module_entry()->suspend(ctx) == 0);
    var.yoffset = 0;
    assert(oa_module_entry()->resume(ctx) == 0);
    assert(display->clear(1, 2, 3) == 0);
    assert(display->present() == 0 && last_yoffset == 0);
    assert(pixels[0] == 3 && pixels[2] == 1);
    finish(ctx);
    puts("  ok   default current-frame presentation commits every time, including after resume");

    ctx = start(1, 0, "1");
    assert(display->present() == 0 && commits == 1 && last_yoffset == 0);
    finish(ctx);
    puts("  ok   single-buffer presentation also commits through FBIOPAN_DISPLAY");

    ctx = start(2, 0, "1");
    fail_sync = 1;
    assert(display->present() == -1 && commits == 0);
    fail_sync = 0;
    fail_commit = 1;
    assert(display->present() == -1 && commits == 1 && last_yoffset == 2);
    fail_commit = 0;
    assert(display->present() == 0 && commits == 2 && last_yoffset == 2);
    assert(display->present() == 0 && commits == 3 && last_yoffset == 0);
    finish(ctx);
    puts("  ok   sync/commit failures propagate and retry the same frame before rotation");

    /* Full-screen transition: an 8x2 scene with red = column*16 is sampled through the recovered
     * clip-space quad (type 2 x-slide, type 7 scale, type 1 keystone). */
    ctx = start(1, 0, NULL);
    for (int x = 0; x < 8; x++)
        assert(display->fill(x, 0, 1, 2, x * 16, 0, 0) == 0);

    assert(display->set_scene_transform(OA_FS_SLIDE_NEG, 0.5f) == 0);
    assert(display->present() == 0);
    assert(pixels[2] == 3);             /* linear sample at column1/3, half opacity */
    assert(pixels[7 * 4 + 2] == 56);    /* X7 samples column7 with half opacity */

    assert(display->set_scene_transform(OA_FS_SCALE, 0.5f) == 0);
    assert(display->present() == 0);
    assert(pixels[2] == 0);             /* X0 outside the 5/6 quad */
    assert(pixels[4 * 4 + 2] == 33);    /* linear sample at column4.1, half opacity */
    assert(pixels[7 * 4 + 2] == 0);

    assert(display->set_scene_transform(OA_FS_SCALE_AXES, 0.5f) == 0);
    assert(display->present() == 0);
    assert(pixels[2] == 1);             /* left edge unchanged; linear subpixel sample */
    assert(pixels[4 * 4 + 2] == 37);    /* keystone: column4.642857, half opacity */
    assert(pixels[7 * 4 + 2] == 0);

    assert(display->set_scene_transform(0, 0.0f) == 0);
    assert(display->present() == 0);
    assert(pixels[4 * 4 + 2] == 64);    /* cleared transform: direct copy of column 4 */
    finish(ctx);
    puts("  ok   scene surface maps the recovered quad (x-slide, scale, keystone, clear)");

    height = 16;
    ctx = start(1, 0, NULL);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 8; x++)
            assert(!display->fill(x, y, 1, 1, x * 16, y * 16, 0));
    assert(!display->set_scene_transform(OA_FS_SCALE_AXES, 0.5f));
    assert(!display->present());
    /* Independently derived barycentric UVs for fan triangles (0,1,2),(0,2,3).
     * At half progress, X3/Y4 samples row3.333333 in the upper triangle;
     * X5/Y5 samples row4.157143 in the lower. GL_LINEAR and half opacity
     * round green to27/33. A bilinear quad warp instead produces29/37. */
    assert(pixels[(4 * 8 + 3) * 4 + 1] == 27);
    assert(pixels[(5 * 8 + 5) * 4 + 1] == 33);
    for (int type = 0; type <= 8; type++) {
        assert(!display->set_scene_transform(type, 1.0f));
        assert(!display->present());
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 8; x++) {
                assert(pixels[(y * 8 + x) * 4 + 1] == y * 16);
                assert(pixels[(y * 8 + x) * 4 + 2] == x * 16);
            }
    }
    finish(ctx);
    puts("  ok   keystone samples both fan triangles; all completed transforms preserve the scene");

    height = 2;
    ctx = start(1, 0, NULL);
    assert(!display->clear(100, 80, 60));
    for (int step = 0; step <= 2; step++) {
        assert(!display->set_scene_transform(OA_FS_UNIFORM_ONLY, (float)step / 2.0f));
        assert(!display->present());
        assert(pixels[0] == step * 30 && pixels[1] == step * 40 && pixels[2] == step * 50);
        assert(pixels[3] == (step == 1 ? 128 : step * 255 / 2));
    }
    finish(ctx);
    puts("  ok   uniform-only fade preserves premultiplied BGRA and transparent-black endpoints");

    /* A full opaque blit has the same pixels as clear+blit, but only the latter
     * establishes the fast-path invariant. Compare both paths at fractional
     * positive/negative offsets, including clamped edge texels. */
    unsigned char rgba[512], reference[15][512];
    for (unsigned i = 0; i < sizeof rgba; i += 4) {
        rgba[i] = (unsigned char)(i * 7);
        rgba[i+1] = (unsigned char)(i * 13);
        rgba[i+2] = (unsigned char)(i * 19);
        rgba[i+3] = 255;
    }
    const float progress[] = {0.125f, 0.37f, 0.875f};
    height = 16;
    ctx = start(1, 0, NULL);
    assert(!display->blit(0, 0, 8, 16, rgba));
    for (int type = 2; type <= 6; type++)
        for (int step = 0; step < 3; step++) {
            assert(!display->set_scene_transform(type, progress[step]) && !display->present());
            memcpy(reference[(type-2)*3+step], pixels, 512);
        }
    assert(!display->clear(0, 0, 0) && !display->blit(0, 0, 8, 16, rgba));
    for (int type = 2; type <= 6; type++)
        for (int step = 0; step < 3; step++) {
            assert(!display->set_scene_transform(type, progress[step]) && !display->present());
            for (unsigned i = 0; i < 512; i++) {
                int delta = (int)pixels[i] - (int)reference[(type-2)*3+step][i];
                assert(delta >= -1 && delta <= 1); /* float arithmetic/UNORM rounding */
            }
        }
    finish(ctx);
    puts("  ok   optimized opaque slides/fade match the general filtered rasterizer within one byte");

    assert(!setenv("OA_TEST_RESTORE", "1", 1));
    height = 2;
    ctx = start(2, 2, NULL); /* snapshot offset2, zero pixels/region/MAU */
    if (!(pixels[0] == 0 && region[2] == 8 && region[3] == 2)) {
        fprintf(stderr, "restore-start pix0=%u region2=%u region3=%u yoff=%u\n",
            pixels[0], region[2], region[3], var.yoffset);
        assert(0);
    }
    var.yoffset = 0;
    assert(!display->clear(10, 20, 30) && !display->present());
    assert(pixels[64] == 30);
    finish(ctx);
    assert(pixels[64] == 0 && var.yoffset == 2);
    assert(region[0] == 0 && region[2] == 0 && region[3] == 0);
    puts("  ok   display restore snapshots/restores pixels, region and pan; MAU verified");

    ctx = start(2, 2, NULL);
    unsigned was_closed = closes;
    assert(!display->clear(10, 20, 30) && !display->present());
    fail_sync = 1;
    assert(oa_module_entry()->suspend(ctx) == -EIO && closes == was_closed);
    assert(!display->clear(40, 50, 60)); /* failed sync suspend leaves live scene */
    fail_sync = 0;
    assert(!oa_module_entry()->suspend(ctx) && closes == was_closed + 1);
    assert(pixels[64] == 0 && var.yoffset == 2);
    assert(!oa_module_entry()->resume(ctx));finish(ctx);
    puts("  ok   display failed suspend retains original snapshot/fd, reports failure and permits retry/resume");

    ctx = start(2, 2, NULL);was_closed = closes;kernel_suspended = 1;
    assert(oa_module_entry()->suspend(ctx) == -EFAULT && closes == was_closed);
    kernel_suspended = 0;
    assert(!oa_module_entry()->suspend(ctx) && var.yoffset == 2);
    finish(ctx);
    puts("  ok   firmware suspended-pan EFAULT retains module snapshot/fd until the kernel permits retry");

    ctx = start(2, 2, NULL);was_closed = closes;
    assert(!display->clear(10, 20, 30) && !display->present());
    create_error = EAGAIN;
    assert(oa_module_entry()->quiesce(ctx) == -EAGAIN && closes == was_closed);
    block_worker = 1;
    assert(oa_module_entry()->quiesce(ctx) == -EAGAIN);
    struct timespec until;assert(!clock_gettime(CLOCK_REALTIME, &until));until.tv_sec += 5;
    pthread_mutex_lock(&gate_mu);
    while (!worker_entered) assert(!pthread_cond_timedwait(&gate_cv, &gate_mu, &until));
    pthread_mutex_unlock(&gate_mu);
    assert(oa_module_entry()->quiesce(ctx) == -EBUSY && closes == was_closed);
    assert(display->clear(0, 0, 0) == -1 && alpha->clear_rgba(0, 0, 0, 0) == -1);
    assert(display->present() == -1 && display->info(NULL, NULL, NULL, NULL) == -1);
    assert(oa_module_entry()->start(ctx) == -EBUSY && oa_module_entry()->resume(ctx) == -EBUSY);
    assert(oa_module_entry()->suspend(ctx) == -EBUSY);
    join_error = EINVAL;
    assert(oa_module_entry()->quiesce(ctx) == -EINVAL && closes == was_closed);
    pthread_mutex_lock(&gate_mu);release_worker = 1;pthread_cond_broadcast(&gate_cv);pthread_mutex_unlock(&gate_mu);
    finish(ctx);block_worker = 0;
    assert(pixels[64] == 0 && var.yoffset == 2);
    puts("  ok   display cleanup gate stays nonblocking during driver wait, retains create/join failures and rejects drawing/restart");

    ctx = start(2, 2, NULL);was_closed = closes;
    assert(!display->clear(10, 20, 30) && !display->present());fail_sync = 1;
    cleanup_error(ctx, -EIO);
    assert(closes == was_closed && display->present() == -1);
    fail_sync = 0;finish(ctx);
    assert(pixels[64] == 0 && var.yoffset == 2);
    puts("  ok   display failed worker restore retains snapshot/fd for throttled retry before stop/destroy");
    assert(!unsetenv("OA_TEST_RESTORE"));

    ctx = start(2, 2, NULL);was_closed = closes;fail_unmap = 1;
    cleanup_error(ctx, -EIO);assert(closes == was_closed && !fail_unmap);
    finish(ctx);assert(closes == was_closed + 1);
    ctx = start(2, 2, NULL);was_closed = closes;fail_close = 1;
    cleanup_error(ctx, -EIO);assert(closes == was_closed + 1 && !fail_close);
    finish(ctx);assert(closes == was_closed + 1); /* never replay consumed fd */
    puts("  ok   display cleanup retains a failed unmap for retry and never replays a consumed close descriptor");
    return 0;
}
