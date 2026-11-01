/*
 * drmprobe - inspect (and optionally paint) the Novatek OSD planes through standard DRM/KMS.
 *
 * The display driver is ntkndpdrm.ko, a normal DRM driver (drm_gem_cma, planes, dumb buffers), so the
 * documented KMS ioctls are enough - no vendor structs needed. This is the candidate replacement for
 * the vendor fb ioctls that DirectFB's tdga driver uses (see artifacts/reports/OSD-INIT-RE-2026-10-30.md).
 *
 *   drmprobe                 # read-only: driver, crtcs, connectors, planes, formats
 *   drmprobe --paint [N]     # also create a dumb buffer and set it on plane N (default: last plane)
 *   drmprobe --paint --crtc 0 --w 1280 --h 720
 *
 * Build (static, no libdrm):
 *   arm-linux-musleabihf-gcc -static -Os -s -o drmprobe drmprobe.c
 *
 * Nothing is destroyed on exit: the plane keeps showing the buffer until the driver owner (aurora)
 * repaints, so re-running with --paint is harmless. Use it with the TV awake.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include <drm/drm.h>
#include <drm/drm_mode.h>

static const char *DEV[] = {"/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2"};

static int xioctl(int fd, unsigned long req, void *arg, const char *what)
{
    int r = ioctl(fd, req, arg);
    if (r != 0)
        printf("  %-28s FAILED (%s)\n", what, strerror(errno));
    else
        printf("  %-28s ok\n", what);
    return r;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0); /* survives SIGKILL diagnostics */
    int paint = 0, want_plane = -1, want_crtc = -1, master = 0, clear = 0;
    unsigned w = 1280, h = 720;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--paint")) {
            paint = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') want_plane = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--crtc") && i + 1 < argc) {
            want_crtc = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--w") && i + 1 < argc) {
            w = (unsigned)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--h") && i + 1 < argc) {
            h = (unsigned)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--master")) {
            master = 1;
        } else if (!strcmp(argv[i], "--clear")) {
            clear = 1;
        } else {
            fprintf(stderr, "usage: %s [--paint [plane]] [--clear [plane]] [--crtc N] [--w N] [--h N] [--master]\n", argv[0]);
            return 2;
        }
    }

    int fd = -1;
    const char *dev = NULL;
    for (unsigned i = 0; i < sizeof DEV / sizeof DEV[0]; i++) {
        fd = open(DEV[i], O_RDWR);
        if (fd >= 0) { dev = DEV[i]; break; }
    }
    if (fd < 0) { perror("open /dev/dri/card*"); return 1; }
    printf("device: %s\n", dev);

    if (master) {
        if (xioctl(fd, DRM_IOCTL_SET_MASTER, NULL, "DRM_IOCTL_SET_MASTER") != 0)
            printf("  (continuing without master; SETPLANE will likely fail)\n");
    }

    struct drm_version ver;
    char name[64] = "", date[64] = "", desc[64] = "";
    memset(&ver, 0, sizeof ver);
    ver.name = name; ver.name_len = sizeof name - 1;
    ver.date = date; ver.date_len = sizeof date - 1;
    ver.desc = desc; ver.desc_len = sizeof desc - 1;
    if (xioctl(fd, DRM_IOCTL_VERSION, &ver, "DRM_IOCTL_VERSION") == 0)
        printf("  driver: %s | %s | %s\n", name, date, desc);

    struct drm_mode_card_res res;
    uint32_t crtcs[16] = {0}, conns[16] = {0}, encs[16] = {0}, fbs[16] = {0};
    memset(&res, 0, sizeof res);
    res.crtc_id_ptr = (unsigned long)crtcs; res.count_crtcs = 16;
    res.connector_id_ptr = (unsigned long)conns; res.count_connectors = 16;
    res.encoder_id_ptr = (unsigned long)encs; res.count_encoders = 16;
    res.fb_id_ptr = (unsigned long)fbs; res.count_fbs = 16;
    if (xioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res, "MODE_GETRESOURCES") != 0) return 1;
    printf("  min %ux%u  max %ux%u  crtcs=%u conns=%u encs=%u fbs=%u\n",
           res.min_width, res.min_height, res.max_width, res.max_height,
           res.count_crtcs, res.count_connectors, res.count_encoders, res.count_fbs);

    for (unsigned c = 0; c < res.count_crtcs && c < 16; c++) {
        struct drm_mode_crtc gc;
        memset(&gc, 0, sizeof gc);
        gc.crtc_id = crtcs[c];
        if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &gc) == 0)
            printf("  crtc %u: id=%u fb=%u %ux%u mode_valid=%u\n", c, gc.crtc_id, gc.fb_id,
                   gc.mode.hdisplay, gc.mode.vdisplay, gc.mode_valid);
    }

    struct drm_mode_get_plane_res pres;
    uint32_t planes[32] = {0};
    memset(&pres, 0, sizeof pres);
    pres.plane_id_ptr = (unsigned long)planes; pres.count_planes = 32;
    if (xioctl(fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &pres, "MODE_GETPLANERESOURCES") != 0) return 1;
    printf("  planes: %u\n", pres.count_planes);

    uint32_t formats[32];
    for (unsigned i = 0; i < pres.count_planes && i < 32; i++) {
        struct drm_mode_get_plane gp;
        memset(&gp, 0, sizeof gp);
        gp.plane_id = planes[i];
        gp.format_type_ptr = (unsigned long)formats; gp.count_format_types = 32;
        if (ioctl(fd, DRM_IOCTL_MODE_GETPLANE, &gp) == 0) {
            printf("  plane %u: id=%u crtc=%u fb=%u possible_crtcs=0x%x formats=",
                   i, gp.plane_id, gp.crtc_id, gp.fb_id, gp.possible_crtcs);
            for (unsigned f = 0; f < gp.count_format_types && f < 8; f++)
                printf("%c%c%c%c ", (char)(formats[f]), (char)(formats[f] >> 8),
                       (char)(formats[f] >> 16), (char)(formats[f] >> 24));
            printf("\n");
        }
    }

    if (clear) {
        unsigned pi = (want_plane >= 0) ? (unsigned)want_plane : 0;
        struct drm_mode_set_plane sp;
        memset(&sp, 0, sizeof sp);
        sp.plane_id = planes[pi];
        printf("  disabling plane %u (id %u)\n", pi, planes[pi]);
        xioctl(fd, DRM_IOCTL_MODE_SETPLANE, &sp, "MODE_SETPLANE(off)");
        if (master) xioctl(fd, DRM_IOCTL_DROP_MASTER, NULL, "DRM_IOCTL_DROP_MASTER");
        close(fd);
        return 0;
    }

    if (!paint) {
        printf("(read-only; add --paint [plane] to try a dumb buffer)\n");
        close(fd);
        return 0;
    }

    struct drm_mode_create_dumb cd;
    memset(&cd, 0, sizeof cd);
    cd.width = w; cd.height = h; cd.bpp = 32;
    if (xioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd, "MODE_CREATE_DUMB") != 0) return 1;
    /* the ndp driver leaves size=0 and only fills pitch: derive it */
    if (cd.size == 0 && cd.pitch)
        cd.size = cd.pitch * h;
    /* cd.size is __u64: %u would shift every later vararg on ARM */
    printf("  dumb: handle=%u pitch=%u size=%llu (%ux%u)%s\n", cd.handle, cd.pitch,
           (unsigned long long)cd.size, w, h, cd.size ? "" : "  [size still 0!]");
    if (cd.size == 0) { printf("  cannot proceed without a buffer size\n"); return 1; }

    struct drm_mode_map_dumb md;
    memset(&md, 0, sizeof md);
    md.handle = cd.handle;
    if (xioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md, "MODE_MAP_DUMB") != 0) return 1;
    printf("  map offset=%llu\n", (unsigned long long)md.offset);
    uint8_t *pix = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, md.offset);
    if (pix == MAP_FAILED) {
        /* the ndp driver rejects the legacy dumb mmap; try the dmabuf (PRIME) path */
        printf("  legacy mmap failed (%s), trying PRIME\n", strerror(errno));
        struct drm_prime_handle ph;
        memset(&ph, 0, sizeof ph);
        ph.handle = cd.handle;
        ph.flags = O_RDWR | O_CLOEXEC;
        if (xioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph, "PRIME_HANDLE_TO_FD") == 0 && ph.fd >= 0) {
            pix = mmap(NULL, cd.size, PROT_READ | PROT_WRITE, MAP_SHARED, ph.fd, 0);
            if (pix == MAP_FAILED) { perror("mmap prime"); return 1; }
            printf("  mapped via dmabuf fd %d\n", ph.fd);
        } else {
            printf("  PRIME path unavailable\n");
            return 1;
        }
    }
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++) {
            uint8_t *p = pix + (size_t)y * cd.pitch + (size_t)x * 4;
            p[0] = (uint8_t)(x * 255 / w);            /* B */
            p[1] = (uint8_t)(y * 255 / h);            /* G */
            p[2] = ((x / 64 + y / 64) & 1) ? 0xFF : 0x00; /* R: checker */
            p[3] = 0xFF;
        }
    printf("  painted %ux%u pattern into the dumb buffer\n", w, h);

    struct drm_mode_fb_cmd fc;
    memset(&fc, 0, sizeof fc);
    fc.width = w; fc.height = h; fc.pitch = cd.pitch; fc.bpp = 32; fc.depth = 24;
    fc.handle = cd.handle;
    if (xioctl(fd, DRM_IOCTL_MODE_ADDFB, &fc, "MODE_ADDFB") != 0) return 1;
    printf("  fb_id=%u\n", fc.fb_id);

    unsigned pi = (want_plane >= 0 && (unsigned)want_plane < pres.count_planes)
                      ? (unsigned)want_plane : pres.count_planes - 1;
    uint32_t crtc = (want_crtc >= 0 && (unsigned)want_crtc < res.count_crtcs)
                        ? crtcs[want_crtc] : (res.count_crtcs ? crtcs[0] : 0);

    struct drm_mode_set_plane sp;
    memset(&sp, 0, sizeof sp);
    sp.plane_id = planes[pi];
    sp.crtc_id = (uint32_t)crtc;
    sp.fb_id = fc.fb_id;
    sp.crtc_x = 0; sp.crtc_y = 0; sp.crtc_w = w; sp.crtc_h = h;
    sp.src_w = w << 16; sp.src_h = h << 16;
    printf("  SETPLANE plane=%u (index %u) crtc=%u fb=%u\n", sp.plane_id, pi, sp.crtc_id, sp.fb_id);
    if (xioctl(fd, DRM_IOCTL_MODE_SETPLANE, &sp, "MODE_SETPLANE") != 0) {
        printf("  (if this is EACCES/EPERM the plane is already owned by the running UI;\n"
               "   if EINVAL, try another plane index)\n");
    }
    munmap(pix, cd.size);
    if (master) xioctl(fd, DRM_IOCTL_DROP_MASTER, NULL, "DRM_IOCTL_DROP_MASTER");
    close(fd);
    return 0;
}
