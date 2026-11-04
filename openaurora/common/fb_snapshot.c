#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "openaurora/fb_snapshot.h"

static int ioctl_error(void) { return errno ? -errno : -EIO; }

static int same_channel(const struct fb_bitfield *a, const struct fb_bitfield *b)
{
    return a->offset == b->offset && a->length == b->length && a->msb_right == b->msb_right;
}

/* FBIOPAN_DISPLAY restores offsets, not modes/allocations. SetPar also changes
 * stride, format and window state in the shipped driver; do not guess a mode
 * rollback with FBIOPUT_VSCREENINFO. Reject changed interpretation/backing
 * before mapping or writing anything. This is a guard, not an ownership lease:
 * the caller must exclude concurrent owners throughout save/use/restore. */
static int same_layout(const struct fb_fix_screeninfo *a, const struct fb_var_screeninfo *v,
    const struct oa_fb_snapshot *s)
{
    const struct fb_fix_screeninfo *b = &s->fix;
    const struct fb_var_screeninfo *w = &s->var;
    return !memcmp(a->id, b->id, sizeof a->id) &&
        a->smem_start == b->smem_start && a->smem_len == b->smem_len &&
        a->line_length == b->line_length && a->type == b->type &&
        a->type_aux == b->type_aux && a->visual == b->visual &&
        v->xres == w->xres && v->yres == w->yres &&
        v->xres_virtual == w->xres_virtual && v->yres_virtual == w->yres_virtual &&
        v->bits_per_pixel == w->bits_per_pixel && v->grayscale == w->grayscale &&
        v->nonstd == w->nonstd && v->rotate == w->rotate && v->colorspace == w->colorspace &&
        same_channel(&v->red, &w->red) && same_channel(&v->green, &w->green) &&
        same_channel(&v->blue, &w->blue) && same_channel(&v->transp, &w->transp) &&
        v->pixclock == w->pixclock && v->left_margin == w->left_margin &&
        v->right_margin == w->right_margin && v->upper_margin == w->upper_margin &&
        v->lower_margin == w->lower_margin && v->hsync_len == w->hsync_len &&
        v->vsync_len == w->vsync_len && v->sync == w->sync && v->vmode == w->vmode;
}

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
    if (!same_layout(&fix, &var, in)) return -ESTALE;
    uint32_t mau = 0;
    if (ioctl(fd, OA_FB_MAU_RD, &mau) != 0) return ioctl_error();
    if (mau != in->mau) return -ESTALE;

    void *map = mmap(NULL, in->fix.smem_len, PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0);
    int rc = 0;
    if (map == MAP_FAILED) rc = ioctl_error();
    else {
        memcpy(map, in->pixels, in->fix.smem_len);
        rc = msync(map, in->fix.smem_len, MS_SYNC) == 0 ? 0 : ioctl_error();
        if (munmap(map, in->fix.smem_len) != 0 && !rc) rc = ioctl_error();
    }

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
