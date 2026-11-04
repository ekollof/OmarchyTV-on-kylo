#ifndef OPENAURORA_FB_SNAPSHOT_H
#define OPENAURORA_FB_SNAPSHOT_H

#include <linux/fb.h>
#include <stdint.h>
#include <sys/ioctl.h>

/* Recovered OSD layer contract: 0x8020c86c reads the opaque 32-byte region,
 * 0x4020c86b writes it. MAU is read-only; there is no setter/callback. */
#define OA_FB_REGION_RD 0x8020c86cu
#define OA_FB_REGION_WR 0x4020c86bu
#define OA_FB_MAU_RD _IOR(0xc8, 0xbd, uint32_t)

struct oa_fb_snapshot {
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    uint32_t region[8];
    uint32_t mau;
    unsigned char *pixels;
};

/* Save/restore pixels, fb var/fix, the 32-byte region and the current MAU.
 * MAU is verified on restore because the kernel exposes no setter. The caller
 * owns the fd; no device is opened or closed here. Returns 0 or -errno. */
int oa_fb_snapshot_save(int fd, struct oa_fb_snapshot *out);
int oa_fb_snapshot_restore(int fd, const struct oa_fb_snapshot *in);
void oa_fb_snapshot_free(struct oa_fb_snapshot *s);

#endif
