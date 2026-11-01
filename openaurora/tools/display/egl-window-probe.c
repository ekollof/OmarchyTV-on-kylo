/* Explicit, temporary real fb1 GPU presentation test. Native-window/MAU/swap
 * semantics must be established from firmware before execution. Saves a complete
 * backup before EGL creation; --restore can recover it after an abnormal exit.
 * Never installed or run by make check. */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#ifdef OA_WINDOW_BROKER
#include "scene-probe-client.h"
#else
#include "openaurora/scene_gles.h"
typedef oa_scene_gles scene_probe;
static int probe_open(scene_probe **scene, unsigned width, unsigned height, const char *helper, const char *renderer)
{ (void)helper; (void)renderer; return oa_scene_gles_open_window(scene, width, height); }
static int probe_render(scene_probe *scene, const uint8_t *rgba, size_t size, int type, float progress)
{ return oa_scene_gles_render(scene, rgba, size, type, progress, NULL, 0); }
#define probe_close oa_scene_gles_close
#define probe_error oa_scene_gles_error
#endif

#ifdef OA_WINDOW_BROKER
#define BACKUP "/tmp/oa-scene-helper-20261101/framebuffer.backup"
#else
#define BACKUP "/tmp/oa-egl-window-20261101/framebuffer.backup"
#endif
#define REGION_GET 0x8020c86cu
#define MAU_GET 0x8004c8bdu
struct snapshot {
    uint32_t magic, version, bytes, mau;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    uint32_t region[8];
};
static volatile sig_atomic_t interrupted;
static void signal_handler(int sig) { interrupted = sig; }
static double millis(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time)) return 0;
    return (double)time.tv_sec*1000 + (double)time.tv_nsec/1000000;
}
static int profile(const struct snapshot *s)
{
    return s->magic == 0x3147574f && s->version == 1 && s->mau == 0 &&
        s->var.xres == 1920 && s->var.yres == 1080 && s->var.xres_virtual == 1920 &&
        s->var.yres_virtual == 2160 && s->var.bits_per_pixel == 32 && !s->var.xoffset &&
        (s->var.yoffset == 0 || s->var.yoffset == 1080) && s->fix.line_length == 7680 &&
        s->fix.smem_len == 16588800 && s->bytes == s->fix.smem_len;
}
static int capture(int fd, struct snapshot *s)
{
    memset(s, 0, sizeof *s);
    s->magic = 0x3147574f; s->version = 1;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &s->var) || ioctl(fd, FBIOGET_FSCREENINFO, &s->fix) ||
        ioctl(fd, REGION_GET, s->region) || ioctl(fd, MAU_GET, &s->mau)) return -1;
    s->bytes = s->fix.smem_len;
    if (!profile(s)) { errno = ENOTSUP; return -1; }
    return 0;
}
static int restore(int fd, uint8_t *map, const struct snapshot *s, const uint8_t *pixels)
{
    /* Refuse to restore into a changed geometry/backing/MAU. No guessed mode or
     * region setter: native MAU switching is explicitly disabled for this run. */
    struct snapshot now;
    if (capture(fd, &now) || now.fix.smem_start != s->fix.smem_start ||
        memcmp(now.region, s->region, sizeof s->region)) { errno = EIO; return -1; }
    memcpy(map, pixels, s->bytes);
    struct fb_var_screeninfo original = s->var;
    if (msync(map, s->bytes, MS_SYNC) || ioctl(fd, FBIOPAN_DISPLAY, &original)) return -1;
    if (capture(fd, &now) || now.var.yoffset != s->var.yoffset || memcmp(map, pixels, s->bytes)) {
        errno = EIO; return -1;
    }
    printf("restored bytes=%u yoffset=%u MAU=%u region_match=1 snapshot_match=1\n",
           s->bytes, now.var.yoffset, now.mau);
    return 0;
}
static int save_backup(const struct snapshot *s, const uint8_t *pixels)
{
    int fd = open(BACKUP, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) return -1;
    FILE *file = fdopen(fd, "wb");
    if (!file) { close(fd); return -1; }
    int failed = fwrite(s, 1, sizeof *s, file) != sizeof *s ||
        fwrite(pixels, 1, s->bytes, file) != s->bytes || fflush(file) || fsync(fd);
    if (fclose(file)) failed = 1;
    return failed ? -1 : 0;
}
static int read_backup(struct snapshot *s, uint8_t *pixels)
{
    int fd = open(BACKUP, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    FILE *file = fdopen(fd, "rb");
    if (!file) { close(fd); return -1; }
    int failed = fread(s, 1, sizeof *s, file) != sizeof *s || !profile(s);
    if (!failed) failed = fread(pixels, 1, s->bytes, file) != s->bytes || fgetc(file) != EOF || ferror(file);
    if (fclose(file)) failed = 1;
    return failed ? -1 : 0;
}
static int error(scene_probe *scene)
{
    char text[160];
    probe_error(scene, text, sizeof text);
    fprintf(stderr, "%s errno=%d\n", text, errno);
    return 1;
}
static int identity_readback(int fd, uint8_t *map, const struct snapshot *s)
{
    struct snapshot now;
    if (capture(fd, &now)) return -1;
    const uint8_t *top = map + (size_t)now.var.yoffset*s->fix.line_length;
    const uint8_t *bottom = top + 1079*s->fix.line_length;
    const uint8_t *right = top + 1919*4;
    printf("identity raw BGRA top=%u,%u,%u,%u bottom=%u,%u,%u,%u right=%u,%u,%u,%u yoffset=%u\n",
        top[0],top[1],top[2],top[3],bottom[0],bottom[1],bottom[2],bottom[3],
        right[0],right[1],right[2],right[3],now.var.yoffset);
    return top[0] == 80 && top[1] == 0 && top[2] == 0 && top[3] == 255 &&
        bottom[0] == 80 && bottom[1] == 224 && bottom[2] == 0 && bottom[3] == 255 &&
        right[0] == 80 && right[1] == 0 && right[2] == 224 && right[3] == 255 ? 0 : -1;
}
int main(int argc, char **argv)
{
    const char *helper = NULL, *renderer = NULL;
#ifdef OA_WINDOW_BROKER
    if (argc == 4 && !strcmp(argv[1], "--broker")) { helper = argv[2]; renderer = argv[3]; }
    else if (argc != 2 || strcmp(argv[1], "--restore")) return 2;
#else
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--restore"))) return 2;
#endif
    int restore_only = argc == 2, result = 1, dirty = 0, closed = 0;
    scene_probe *scene = NULL;
    struct snapshot saved;
    int fd = open("/dev/fb1", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("open fb1"); return 1; }
    if (capture(fd, &saved)) { perror("framebuffer profile"); close(fd); return 1; }
    uint8_t *map = mmap(NULL, saved.bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint8_t *pixels = malloc(saved.bytes), *input = malloc(1920u*1080u*4);
    if (map == MAP_FAILED || !pixels || !input) {
        if (map != MAP_FAILED) munmap(map, saved.bytes);
        free(pixels); free(input); close(fd); return 1;
    }
    if (restore_only) {
        struct snapshot backup;
        if (!read_backup(&backup, pixels) && backup.fix.smem_start == saved.fix.smem_start)
            result = restore(fd, map, &backup, pixels) != 0;
        goto cleanup;
    }
    memcpy(pixels, map, saved.bytes);
    if (save_backup(&saved, pixels)) { perror("backup"); goto cleanup; }
    printf("saved bytes=%u original_yoffset=%u MAU=%u backup=%s\n",
           saved.bytes, saved.var.yoffset, saved.mau, BACKUP);
    fflush(stdout);
    struct sigaction action = {.sa_handler = signal_handler};
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL) ||
        sigaction(SIGHUP, &action, NULL)) goto cleanup;
    /* Firmware477fed74 parses per-fb comma-separated switches. No alternate
     * MAU or new buffer/mode policy is tested here; these env vars are process-only. */
    if (setenv("FRAMEBUFFER_2", "/dev/fb1", 1) || setenv("MALI_MAU_SWITCH", "0,0,0,0,0", 1)) goto cleanup;
    for (unsigned y = 0; y < 1080; y++)
        for (unsigned x = 0; x < 1920; x++) {
            uint8_t *p = input + ((size_t)y*1920+x)*4;
            p[0] = (uint8_t)((x/240)*32); p[1] = (uint8_t)((y/135)*32); p[2] = 80; p[3] = 255;
        }
    dirty = 1;
    if (probe_open(&scene, 1920, 1080, helper, renderer)) { (void)error(scene); goto cleanup; }
#ifdef OA_WINDOW_BROKER
    int ui_result = probe_ui(scene);
    if (ui_result < 0) { (void)error(scene); goto cleanup; }
    if (ui_result > 0) { result = 0; goto cleanup; }
#endif
    for (int i = 0; i < 2; i++)
        if (probe_render(scene, i ? NULL : input, i ? 0 : 1920u*1080u*4,
                                 0, 1)) { (void)error(scene); goto cleanup; }
    struct timespec pause = {.tv_nsec = 100000000};
    (void)nanosleep(&pause, NULL);
    if (identity_readback(fd, map, &saved)) { fputs("native identity/orientation mismatch\n", stderr); goto cleanup; }
#ifdef OA_WINDOW_BROKER
    if (probe_coalesce(scene)) { (void)error(scene); goto cleanup; }
#endif
    for (int type = 1; type <= 8 && !interrupted; type++) {
        double began = millis(), elapsed = 0;
        unsigned frames = 0;
        while (elapsed < 1000 && !interrupted && frames < 200) {
            float p = (float)(elapsed/1000), d = p-1;
            float eased = 1+d*d*d;
            if (probe_render(scene, NULL, 0, type, eased)) {
                (void)error(scene); goto cleanup;
            }
            frames++;
            /* Target50Hz without adding a fixed sleep to a blocking swap. */
            double due = began + (double)frames*20;
            struct timespec deadline = {.tv_sec = (time_t)(due/1000),
                .tv_nsec = (long)((due-(double)(time_t)(due/1000)*1000)*1000000)};
            int wait = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
            if (wait && wait != EINTR) { errno = wait; perror("frame pacing"); goto cleanup; }
            elapsed = millis()-began;
        }
        if (probe_render(scene, NULL, 0, type, 1)) {
            (void)error(scene); goto cleanup;
        }
        printf("native type=%d frames=%u elapsed_ms=%.2f\n", type, frames+1, millis()-began);
        fflush(stdout);
    }
    result = interrupted ? 128+interrupted : 0;
cleanup:
    for (int attempt = 0; attempt < 3; attempt++)
        if (!probe_close(&scene)) { closed = 1; break; }
    if (!closed) { (void)error(scene); result = 1; }
    if (dirty && (!closed || restore(fd, map, &saved, pixels))) {
        perror("restore failed; retain backup and use --restore after probe exits"); result = 1;
    }
    free(pixels); free(input); munmap(map, saved.bytes); close(fd);
    return result;
}
