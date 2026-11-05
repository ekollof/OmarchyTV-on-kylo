/* Read-only fb1 region sampler for a live vendor transition.
 *
 *   fb-region-sampler DEV OUTDIR X Y W H WAIT_MS INTERVAL_MS CAPTURE_MS
 *
 * Opens the framebuffer O_RDONLY, waits for a change in the selected region,
 * keeps an 80 ms ring of pre-trigger frames, then writes the pulse frames as
 * BGRA patches plus a manifest. It never writes to the framebuffer, never
 * changes pan/region/MAU and never opens an input device.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RING_FRAMES 10
#define MAX_BYTES (1920u * 1080u * 4u)

static int64_t now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return 0;
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint64_t fnv(const uint8_t *p, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

int main(int argc, char **argv)
{
    if (argc != 10) {
        fprintf(stderr, "usage: %s DEV OUTDIR X Y W H WAIT_MS INTERVAL_MS CAPTURE_MS\n", argv[0]);
        return 2;
    }
    const char *dev = argv[1], *outdir = argv[2];
    int x = atoi(argv[3]), y = atoi(argv[4]);
    int w = atoi(argv[5]), h = atoi(argv[6]);
    int wait_ms = atoi(argv[7]), interval_ms = atoi(argv[8]), capture_ms = atoi(argv[9]);
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || (size_t)w * (size_t)h * 4u > MAX_BYTES ||
        wait_ms < 1 || wait_ms > 120000 || interval_ms < 2 || interval_ms > 100 ||
        capture_ms < 1 || capture_ms > 3000) {
        fprintf(stderr, "invalid region/timing\n");
        return 2;
    }
    size_t bytes = (size_t)w * (size_t)h * 4u;
    int fd = open(dev, O_RDONLY | O_CLOEXEC);
    if (fd < 0) { perror(dev); return 2; }
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) || ioctl(fd, FBIOGET_FSCREENINFO, &fix) ||
        var.bits_per_pixel != 32 || !var.xres || !var.yres ||
        (unsigned)(x + w) > var.xres || (unsigned)(y + h) > var.yres ||
        !fix.smem_len || !fix.line_length) {
        fprintf(stderr, "%s: unsupported framebuffer geometry\n", dev);
        close(fd);
        return 2;
    }
    uint8_t *map = mmap(NULL, fix.smem_len, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); close(fd); return 2; }
    if (mkdir(outdir, 0700) && errno != EEXIST) { perror(outdir); munmap(map, fix.smem_len); close(fd); return 2; }

    char path[512];
    snprintf(path, sizeof path, "%s/frames.bgra", outdir);
    FILE *frames = fopen(path, "wb");
    snprintf(path, sizeof path, "%s/manifest.txt", outdir);
    FILE *manifest = fopen(path, "w");
    uint8_t *cur = malloc(bytes);
    uint8_t *ring[RING_FRAMES] = {0};
    uint64_t rhash[RING_FRAMES] = {0};
    int64_t rtime[RING_FRAMES] = {0};
    unsigned ryoff[RING_FRAMES] = {0};
    if (!frames || !manifest || !cur) { fprintf(stderr, "allocation/output failure\n"); goto fail; }
    for (unsigned i = 0; i < RING_FRAMES; i++)
        if (!(ring[i] = malloc(bytes))) { fprintf(stderr, "allocation failure\n"); goto fail; }

    unsigned line = fix.line_length;
    unsigned yoff = var.yoffset;
    int64_t start = now_ms(), trigger = 0;
    unsigned saved = 0, ring_count = 0, prev_hash_set = 0;
    uint64_t prev_hash = 0;
    while (now_ms() - start < wait_ms) {
        if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0) yoff = var.yoffset;
        if (yoff + (unsigned)h > var.yres_virtual) yoff = 0;
        const uint8_t *base = map + (size_t)yoff * line + (size_t)y * line + (size_t)x * 4u;
        for (int row = 0; row < h; row++)
            memcpy(cur + (size_t)row * w * 4u, base + (size_t)row * line, (size_t)w * 4u);
        uint64_t hash = fnv(cur, bytes);
        if (prev_hash_set && hash != prev_hash) {
            trigger = now_ms();
            break;
        }
        prev_hash = hash; prev_hash_set = 1;
        unsigned slot = ring_count % RING_FRAMES;
        memcpy(ring[slot], cur, bytes);
        rhash[slot] = hash; rtime[slot] = now_ms(); ryoff[slot] = yoff;
        ring_count++;
        struct timespec pause = { .tv_nsec = (long)interval_ms * 1000000L };
        nanosleep(&pause, NULL);
    }
    if (!trigger) {
        fprintf(stderr, "no change within %d ms\n", wait_ms);
        goto fail;
    }
    unsigned pre = ring_count < RING_FRAMES ? ring_count : RING_FRAMES;
    for (unsigned i = 0; i < pre; i++) {
        unsigned slot = (ring_count - pre + i) % RING_FRAMES;
        if (fwrite(ring[slot], 1, bytes, frames) != bytes) goto fail;
        fprintf(manifest, "%u %lld %u %016llx\n", saved++,
                (long long)(rtime[slot] - trigger), ryoff[slot],
                (unsigned long long)rhash[slot]);
    }
    int64_t end = trigger + capture_ms;
    while (now_ms() < end) {
        if (ioctl(fd, FBIOGET_VSCREENINFO, &var) == 0) yoff = var.yoffset;
        if (yoff + (unsigned)h > var.yres_virtual) yoff = 0;
        const uint8_t *base = map + (size_t)yoff * line + (size_t)y * line + (size_t)x * 4u;
        for (int row = 0; row < h; row++)
            memcpy(cur + (size_t)row * w * 4u, base + (size_t)row * line, (size_t)w * 4u);
        uint64_t hash = fnv(cur, bytes);
        if (fwrite(cur, 1, bytes, frames) != bytes) goto fail;
        fprintf(manifest, "%u %lld %u %016llx\n", saved++,
                (long long)(now_ms() - trigger), yoff, (unsigned long long)hash);
        struct timespec pause = { .tv_nsec = (long)interval_ms * 1000000L };
        nanosleep(&pause, NULL);
    }
    fprintf(manifest, "# region %d,%d %dx%d interval=%d capture=%u\n", x, y, w, h,
            interval_ms, saved);
    printf("captured %u frames %dx%d in %s\n", saved, w, h, outdir);
    fclose(frames); fclose(manifest);
    for (unsigned i = 0; i < RING_FRAMES; i++) free(ring[i]);
    free(cur); munmap(map, fix.smem_len); close(fd);
    return 0;

fail:
    if (frames) fclose(frames);
    if (manifest) fclose(manifest);
    for (unsigned i = 0; i < RING_FRAMES; i++) free(ring[i]);
    free(cur); munmap(map, fix.smem_len); close(fd);
    return 3;
}
