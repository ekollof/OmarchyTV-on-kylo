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

/* Save pixels, var/fix metadata, the 32-byte region and current MAU. Restore
 * pixels/region/pan only when backing/layout/mode and MAU still match; otherwise
 * -ESTALE without writes. No mode/allocation/MAU setter is invoked. Mapping or
 * sync failures still attempt region/pan, returning the first error. A failure
 * can leave partial restoration; this function retains the snapshot for retry.
 * Caller exclusively owns the fd/device through the whole transaction; these
 * GET-based guards do not exclude another owner racing them, nor detect an
 * allocation replaced at the same physical address. Free an existing snapshot
 * before saving again. No device is opened/closed. Returns 0 or -errno. */
int oa_fb_snapshot_save(int fd, struct oa_fb_snapshot *out);
int oa_fb_snapshot_restore(int fd, const struct oa_fb_snapshot *in);
void oa_fb_snapshot_free(struct oa_fb_snapshot *s);

#endif
