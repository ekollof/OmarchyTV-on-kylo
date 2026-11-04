/* Save/restore a framebuffer's full temporary-test state: pixels, var/fix,
 * the 32-byte OSD region and the current MAU. This is a local hardware helper,
 * not part of the module runtime.
 *
 *   fb-restore save /dev/fb1 FILE
 *   fb-restore restore /dev/fb1 FILE
 *   fb-restore pan /dev/fb1 YOFFSET
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
#include <unistd.h>

#include "openaurora/fb_snapshot.h"

#define MAGIC 0x32524246u /* "FBR2" */

struct header {
    uint32_t magic;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    uint32_t region[8];
    uint32_t mau;
};

static int save(const char *dev, const char *path)
{
    int fd = open(dev, O_RDONLY);
    if (fd < 0) { perror(dev); return 1; }
    struct oa_fb_snapshot snap;
    int rc = oa_fb_snapshot_save(fd, &snap);
    if (rc) {
        fprintf(stderr, "%s: snapshot failed: %s\n", dev, strerror(-rc));
        close(fd);
        return 1;
    }
    struct header h = { MAGIC, snap.var, snap.fix, {0}, snap.mau };
    memcpy(h.region, snap.region, sizeof h.region);
    FILE *out = fopen(path, "wb");
    if (!out || fwrite(&h, sizeof h, 1, out) != 1 ||
        fwrite(snap.pixels, 1, snap.fix.smem_len, out) != snap.fix.smem_len) {
        fprintf(stderr, "%s: short write\n", path);
        if (out) fclose(out);
        oa_fb_snapshot_free(&snap);
        close(fd);
        return 1;
    }
    fclose(out);
    printf("saved %s: %ux%u virt %ux%u %ubpp offset %u,%u bytes %u "
           "mau %u region[0..1] %#x/%#x\n",
           dev, snap.var.xres, snap.var.yres, snap.var.xres_virtual,
           snap.var.yres_virtual, snap.var.bits_per_pixel, snap.var.xoffset,
           snap.var.yoffset, snap.fix.smem_len, snap.mau,
           snap.region[0], snap.region[1]);
    oa_fb_snapshot_free(&snap);
    close(fd);
    return 0;
}

static int restore(const char *dev, const char *path)
{
    FILE *in = fopen(path, "rb");
    if (!in) { perror(path); return 1; }
    struct header h;
    if (fread(&h, sizeof h, 1, in) != 1 || h.magic != MAGIC ||
        h.fix.smem_len == 0) {
        fprintf(stderr, "%s: bad header\n", path);
        fclose(in);
        return 1;
    }
    unsigned char *pixels = malloc(h.fix.smem_len);
    if (!pixels || fread(pixels, 1, h.fix.smem_len, in) != h.fix.smem_len) {
        fprintf(stderr, "%s: short read\n", path);
        free(pixels);
        fclose(in);
        return 1;
    }
    fclose(in);
    struct oa_fb_snapshot snap = { h.var, h.fix, {0}, h.mau, pixels };
    memcpy(snap.region, h.region, sizeof snap.region);
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror(dev); free(pixels); return 1; }
    int rc = oa_fb_snapshot_restore(fd, &snap);
    if (rc)
        fprintf(stderr, "%s: restore failed: %s\n", dev, strerror(-rc));
    else
        printf("restored %s: bytes %u offset %u,%u mau %u\n", dev,
               h.fix.smem_len, h.var.xoffset, h.var.yoffset, h.mau);
    close(fd);
    free(pixels);
    return rc ? 1 : 0;
}

static int pan(const char *dev, const char *arg)
{
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror(dev); return 1; }
    struct fb_var_screeninfo v;
    memset(&v, 0, sizeof v);
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v) != 0) { close(fd); return 1; }
    v.yoffset = (uint32_t)strtoul(arg, NULL, 0);
    int rc = ioctl(fd, FBIOPAN_DISPLAY, &v);
    printf("pan %s: yoffset %u %s\n", dev, v.yoffset,
           rc == 0 ? "ok" : strerror(errno));
    close(fd);
    return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "save")) return save(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "restore")) return restore(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "pan")) return pan(argv[2], argv[3]);
    fprintf(stderr, "usage: %s save|restore DEV FILE | pan DEV YOFFSET\n", argv[0]);
    return 2;
}
