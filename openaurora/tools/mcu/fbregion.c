/*
 * fbregion — read back the OSD layer state the vendor driver keeps.
 *
 * The 32-byte region struct is opaque in the decompile, but ioctl 0x8020c86c returns exactly what the
 * driver has, and while vendor aurora is showing its portal that state is known-good. Reading it is
 * how the takeover gets the right fields instead of guessing them.
 *
 *   fbregion [/dev/fb1]
 */
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define OSD_REG_RD 0x8020c86cu
/* _ker_osd_fb_Ioctl masks command to its low byte. Case 0xbd copies one
 * uint32_t from gu32_current_mau[plane] to userspace, with no setter/callback.
 * See OSD-MAU-SELECTION-2026-10-31.md for the recovered driver contract. */
#define OSD_CURRENT_MAU _IOR(0xc8, 0xbd, uint32_t)

int main(int argc, char **argv)
{
    const char *dev = argc > 1 ? argv[1] : "/dev/fb1";
    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        perror(dev);
        return 1;
    }

    struct fb_var_screeninfo v;
    memset(&v, 0, sizeof v);
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v) == 0)
        printf("%s: %ux%u virt %ux%u bpp %u offset %u,%u\n", dev, v.xres, v.yres, v.xres_virtual,
               v.yres_virtual, v.bits_per_pixel, v.xoffset, v.yoffset);

    struct fb_fix_screeninfo fix;
    memset(&fix, 0, sizeof fix);
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0) {
        perror("fixed screeninfo read");
        close(fd);
        return 1;
    }
    printf("primary mapping: physical=%#lx bytes=%u stride=%u\n",
           (unsigned long)fix.smem_start, fix.smem_len, fix.line_length);
    uint32_t mau = 0;
    if (ioctl(fd, OSD_CURRENT_MAU, &mau) != 0) {
        perror("current MAU read");
        close(fd);
        return 1;
    }
    printf("current MAU: %u%s\n", mau, mau ? " (alternate memory selected)" : " (primary memory selected)");

    unsigned int reg[8];
    memset(reg, 0, sizeof reg);
    if (ioctl(fd, OSD_REG_RD, reg) != 0) {
        perror("region read");
        close(fd);
        return 1;
    }
    printf("region:");
    for (int i = 0; i < 8; i++)
        printf(" [%d]=%#x(%u)", i, reg[i], reg[i]);
    printf("\n");
    close(fd);
    return 0;
}
