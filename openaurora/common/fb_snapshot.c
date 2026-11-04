#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "openaurora/fb_snapshot.h"

static int ioctl_error(void) { return errno ? -errno : -EIO; }

int oa_fb_snapshot_save(int fd, struct oa_fb_snapshot *out)
{
    if (fd < 0 || !out) return -EINVAL;
    memset(out, 0, sizeof *out);
    if (ioctl(fd, FBIOGET_VSCREENINFO, &out->var) != 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &out->fix) != 0)
        return ioctl_error();
    if (!out->fix.smem_len || !out->fix.line_length || !out->var.yres ||
        !out->var.bits_per_pixel)
        return -EINVAL;
    if (ioctl(fd, OA_FB_REGION_RD, out->region) != 0 ||
        ioctl(fd, OA_FB_MAU_RD, &out->mau) != 0)
        return ioctl_error();
    void *map = mmap(NULL, out->fix.smem_len, PROT_READ, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) return ioctl_error();
    out->pixels = malloc(out->fix.smem_len);
    if (!out->pixels) {
        munmap(map, out->fix.smem_len);
        return -ENOMEM;
    }
    memcpy(out->pixels, map, out->fix.smem_len);
    if (munmap(map, out->fix.smem_len) != 0) {
        int rc = ioctl_error();
        oa_fb_snapshot_free(out);
        return rc;
    }
    return 0;
}

int oa_fb_snapshot_restore(int fd, const struct oa_fb_snapshot *in)
{
    if (fd < 0 || !in || !in->pixels || !in->fix.smem_len) return -EINVAL;
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0)
        return ioctl_error();
    if (fix.smem_len < in->fix.smem_len) return -EINVAL;
    uint32_t mau = 0;
    if (ioctl(fd, OA_FB_MAU_RD, &mau) != 0) return ioctl_error();
    if (mau != in->mau) return -ESTALE;

    void *map = mmap(NULL, in->fix.smem_len, PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) return ioctl_error();
    memcpy(map, in->pixels, in->fix.smem_len);
    int rc = msync(map, in->fix.smem_len, MS_SYNC) == 0 ? 0 : ioctl_error();
    if (munmap(map, in->fix.smem_len) != 0 && !rc) rc = ioctl_error();

    if (ioctl(fd, OA_FB_REGION_WR, in->region) != 0 && !rc) rc = ioctl_error();
    var.xoffset = in->var.xoffset;
    var.yoffset = in->var.yoffset;
    if (ioctl(fd, FBIOPAN_DISPLAY, &var) != 0 && !rc) rc = ioctl_error();
    if (!rc) {
        if (ioctl(fd, OA_FB_MAU_RD, &mau) != 0) rc = ioctl_error();
        else if (mau != in->mau) rc = -ESTALE;
    }
    return rc;
}

void oa_fb_snapshot_free(struct oa_fb_snapshot *s)
{
    if (!s) return;
    free(s->pixels);
    s->pixels = NULL;
}
