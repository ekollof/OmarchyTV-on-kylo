/* Temporary, real-fbdev scene test. Leaves the vendor process running and restores
 * the saved framebuffer bytes and original pan on every normal/error/signal exit.
 * Compile with modules/display/display.c and OA_NO_VENDOR: only its fbdev path
 * runs, never a second vendor platform/HAL initialization. No region writes.
 * Run only in an owner-authorized drawing window. Readback is not scanout proof.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

const oa_module *oa_module_entry(void);
static const oa_display_iface *display;
static volatile sig_atomic_t interrupted;
static void on_signal(int sig) { interrupted = sig; }
static void log_message(void *p, int level, const char *fmt, ...)
{
    (void)p; (void)level;
    va_list args;
    va_start(args, fmt); vfprintf(stderr, fmt, args); va_end(args);
    fputc('\n', stderr);
}
static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "display.fb")) return "/dev/fb1";
    if (!strcmp(key, "display.set_region") || !strcmp(key, "display.mirror") ||
        !strcmp(key, "display.pan") || !strcmp(key, "display.open_timeout_ms")) return "0";
    return def;
}
static int publish(void *p, const char *name, uint32_t version, const void *iface)
{
    (void)p;
    if (!strcmp(name, OA_SVC_DISPLAY) && version == 2) { display = iface; return 0; }
    if (!strcmp(name, OA_SVC_DISPLAY_ALPHA) && version == 1) return 0; /* not exercised here */
    return -1;
}
static int unpublish(void *p, const char *name)
{
    (void)p;
    if (!strcmp(name, OA_SVC_DISPLAY)) display = NULL;
    else if (strcmp(name, OA_SVC_DISPLAY_ALPHA)) return -1;
    return 0;
}
static void release(void *p, const char *name) { (void)p; (void)name; }
static double millis(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return 0;
    return (double)t.tv_sec * 1000.0 + (double)t.tv_nsec / 1000000.0;
}
int main(void)
{
    int result = 1, started = 0, dirty = 0;
    void *ctx = NULL;
    struct fb_var_screeninfo original;
    struct fb_fix_screeninfo fix;
    const oa_module *module = oa_module_entry();
    const oa_host_api api = {.size = sizeof api, .abi = OA_ABI_VERSION,
        .log = log_message, .config_get = config, .publish = publish,
        .unpublish = unpublish, .release = release};
    int fd = open("/dev/fb1", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("open fb1"); return 1; }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &original) || ioctl(fd, FBIOGET_FSCREENINFO, &fix)) {
        perror("fb metadata"); close(fd); return 1;
    }
    printf("fb1 %ux%u virtual=%ux%u offset=%u,%u bpp=%u stride=%u memory=%u\n",
        original.xres, original.yres, original.xres_virtual, original.yres_virtual,
        original.xoffset, original.yoffset, original.bits_per_pixel, fix.line_length, fix.smem_len);
    /* Deliberately narrow to this established target profile before any write. */
    if (original.bits_per_pixel != 32 || original.xres != 1920 || original.yres != 1080 ||
        original.xoffset || original.yoffset % original.yres ||
        original.yoffset + original.yres > original.yres_virtual ||
        fix.line_length < original.xres * 4 || !original.yres_virtual ||
        fix.line_length > 64u * 1024u * 1024u / original.yres_virtual) {
        fputs("unsupported framebuffer profile; no write\n", stderr); close(fd); return 1;
    }
    size_t length = (size_t)fix.line_length * original.yres_virtual;
    if (length > fix.smem_len) { fputs("mapping exceeds backing; no write\n", stderr); close(fd); return 1; }
    uint8_t *map = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint8_t *saved = malloc(length);
    if (map == MAP_FAILED || !saved) {
        perror("snapshot"); if (map != MAP_FAILED) munmap(map, length);
        free(saved); close(fd); return 1;
    }
    memcpy(saved, map, length);
    struct sigaction action = {.sa_handler = on_signal};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL) ||
        sigaction(SIGHUP, &action, NULL)) goto cleanup;
    if (module->init(&api, &ctx)) goto cleanup;
    started = 1; /* stop also cleans a partial start */
    if (module->start(ctx) || !display) goto cleanup;
    if (display->clear(20, 40, 80)) goto cleanup;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            if (display->fill(x * 240, y * 135, 240, 135, x * 32, y * 32, 80)) goto cleanup;
    dirty = 1;
    for (int type = 0; type <= 8 && !interrupted; type++) {
        double began = millis();
        for (int step = 0; step <= 4 && !interrupted; step++) {
            if (display->set_scene_transform(type, (float)step / 4.0f) || display->present()) {
                perror("scene present"); goto cleanup;
            }
            /* Identity endpoint verifies RGBA->BGRA and current-frame commit. */
            if (step == 4) {
                const uint8_t *p = map + (size_t)original.yoffset * fix.line_length;
                if (p[0] != 80 || p[1] != 0 || p[2] != 0 || p[3] != 255) {
                    fputs("identity readback mismatch\n", stderr); goto cleanup;
                }
            }
        }
        printf("type=%d five presents elapsed_ms=%.2f\n", type, millis() - began);
        fflush(stdout);
    }
    result = interrupted ? 128 + interrupted : 0;
cleanup:
    /* Direct ABI3 caller: retain module/context until asynchronous device
     * cleanup has actually joined, just as the host's stop/unload gate does. */
    if (ctx && module->quiesce)
        while (module->quiesce(ctx)) usleep(10000);
    if (started) module->stop(ctx);
    if (ctx) module->destroy(ctx);
    if (dirty) {
        memcpy(map, saved, length);
        if (msync(map, length, MS_SYNC) || ioctl(fd, FBIOPAN_DISPLAY, &original)) {
            perror("restore framebuffer"); result = 1;
        } else {
            printf("restored %zu bytes and original yoffset=%u; snapshot_match=%d\n",
                   length, original.yoffset, !memcmp(map, saved, length));
        }
    }
    free(saved); munmap(map, length); close(fd);
    return result;
}
