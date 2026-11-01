/* Save/restore a framebuffer's pixels and pan offset for a temporary fb1 test.
 * This is a local hardware helper, not part of the module runtime.
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

#define MAGIC 0x31524246u /* "FBR1" */

struct header {
    uint32_t magic;
    uint32_t xres, yres, xres_virtual, yres_virtual;
    uint32_t bits_per_pixel, xoffset, yoffset;
    uint32_t smem_len;
};

static int get_var(int fd, struct fb_var_screeninfo *v, struct fb_fix_screeninfo *f)
{
    memset(v, 0, sizeof *v);
    memset(f, 0, sizeof *f);
    return ioctl(fd, FBIOGET_VSCREENINFO, v) == 0 && ioctl(fd, FBIOGET_FSCREENINFO, f) == 0 ? 0 : -1;
}

static int save(const char *dev, const char *path)
{
    int fd = open(dev, O_RDONLY);
    if (fd < 0) { perror(dev); return 1; }
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (get_var(fd, &v, &f) != 0 || f.smem_len == 0) {
        fprintf(stderr, "%s: no framebuffer info\n", dev);
        close(fd);
        return 1;
    }
    unsigned char *map = mmap(NULL, f.smem_len, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    struct header h = { MAGIC, v.xres, v.yres, v.xres_virtual, v.yres_virtual,
                        v.bits_per_pixel, v.xoffset, v.yoffset, f.smem_len };
    FILE *out = fopen(path, "wb");
    if (!out || fwrite(&h, sizeof h, 1, out) != 1 || fwrite(map, 1, f.smem_len, out) != f.smem_len) {
        fprintf(stderr, "%s: short write\n", path);
        if (out) fclose(out);
        munmap(map, f.smem_len);
        close(fd);
        return 1;
    }
    fclose(out);
    munmap(map, f.smem_len);
    close(fd);
    printf("saved %s: %ux%u virt %ux%u %ubpp offset %u,%u bytes %u\n", dev, v.xres, v.yres,
           v.xres_virtual, v.yres_virtual, v.bits_per_pixel, v.xoffset, v.yoffset, f.smem_len);
    return 0;
}

static int restore(const char *dev, const char *path)
{
    FILE *in = fopen(path, "rb");
    if (!in) { perror(path); return 1; }
    struct header h;
    if (fread(&h, sizeof h, 1, in) != 1 || h.magic != MAGIC || h.smem_len == 0) {
        fprintf(stderr, "%s: bad header\n", path);
        fclose(in);
        return 1;
    }
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror(dev); fclose(in); return 1; }
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (get_var(fd, &v, &f) != 0 || f.smem_len < h.smem_len) {
        fprintf(stderr, "%s: framebuffer shrank\n", dev);
        close(fd);
        fclose(in);
        return 1;
    }
    unsigned char *map = mmap(NULL, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { perror("mmap"); close(fd); fclose(in); return 1; }
    if (fread(map, 1, h.smem_len, in) != h.smem_len) {
        fprintf(stderr, "%s: short read\n", path);
        munmap(map, f.smem_len);
        close(fd);
        fclose(in);
        return 1;
    }
    msync(map, h.smem_len, MS_SYNC);
    munmap(map, f.smem_len);
    v.xoffset = h.xoffset;
    v.yoffset = h.yoffset;
    if (ioctl(fd, FBIOPAN_DISPLAY, &v) != 0)
        perror("FBIOPAN_DISPLAY");
    close(fd);
    fclose(in);
    printf("restored %s: bytes %u offset %u,%u\n", dev, h.smem_len, h.xoffset, h.yoffset);
    return 0;
}

static int pan(const char *dev, const char *arg)
{
    int fd = open(dev, O_RDWR);
    if (fd < 0) { perror(dev); return 1; }
    struct fb_var_screeninfo v;
    struct fb_fix_screeninfo f;
    if (get_var(fd, &v, &f) != 0) { close(fd); return 1; }
    v.yoffset = (uint32_t)strtoul(arg, NULL, 0);
    int rc = ioctl(fd, FBIOPAN_DISPLAY, &v);
    printf("pan %s: yoffset %u %s\n", dev, v.yoffset, rc == 0 ? "ok" : strerror(errno));
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
