/* oa-ir-blast — process-isolated NDAL BLAST receiver.
 *
 * The vendor NDAL_VideoExt_BLAST_RX sleeps in blast_io_get_key while the
 * remote is idle and is not interrupted by closing the v4l2 fd. Run it in this
 * child process and write fixed oa_ir_blast_record bytes to stdout; mod_ir can
 * kill/reap the child on stop and no vendor call remains in the host loop.
 */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "openaurora/ir_blast.h"

static int write_all(int fd, const void *data, size_t size)
{
    const unsigned char *p = data;
    while (size) {
        ssize_t n = write(fd, p, size);
        if (n > 0) {
            p += n;
            size -= (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        return -1;
    }
    return 0;
}

static void *sym(void *lib, const char *name)
{
    void *p = dlsym(lib, name);
    return p;
}

int main(int argc, char **argv)
{
    const char *video = "/dev/video28";
    unsigned protocol = 4;
    int set_protocol = 1;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [--video PATH] [--protocol N] [--set 0|1]\n",
                   argv[0]);
            return 0;
        }
        if (!strcmp(argv[i], "--video") && i + 1 < argc) {
            video = argv[++i];
        } else if (!strcmp(argv[i], "--protocol") && i + 1 < argc) {
            char *end = NULL;
            unsigned long v = strtoul(argv[++i], &end, 0);
            if (!end || *end || v > 0xff) {
                fprintf(stderr, "oa-ir-blast: invalid protocol\n");
                return 2;
            }
            protocol = (unsigned)v;
        } else if (!strcmp(argv[i], "--set") && i + 1 < argc) {
            set_protocol = atoi(argv[++i]) != 0;
        } else {
            fprintf(stderr, "oa-ir-blast: unknown argument %s\n", argv[i]);
            return 2;
        }
    }

    (void)dlopen("libasound.so.2", RTLD_NOW | RTLD_GLOBAL);
    void *v4l2 = dlopen("libv4l2.so.0", RTLD_NOW | RTLD_GLOBAL);
    int (*v4l2_open_fn)(const char *, int, ...) = NULL;
    if (v4l2) {
        void *p = sym(v4l2, "v4l2_open");
        if (p)
            memcpy(&v4l2_open_fn, &p, sizeof p);
    }

    void *lib = dlopen("libNDAL_EX.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        fprintf(stderr, "oa-ir-blast: libNDAL_EX.so: %s\n", dlerror());
        return 1;
    }
    int (*set)(unsigned) = NULL;
    int (*rx)(uint32_t *, uint8_t *, uint8_t *) = NULL;
    void (*setfd)(int) = NULL;
    void *p = sym(lib, "NDAL_VideoExt_BLAST_SetRcProtocol");
    if (p) memcpy(&set, &p, sizeof p);
    p = sym(lib, "NDAL_VideoExt_BLAST_RX");
    if (p) memcpy(&rx, &p, sizeof p);
    p = sym(lib, "ndcommon_v4l2_set_fd");
    if (p) memcpy(&setfd, &p, sizeof p);
    if (!rx) {
        fprintf(stderr, "oa-ir-blast: NDAL_VideoExt_BLAST_RX missing\n");
        return 1;
    }

    if (video[0]) {
        int fd = v4l2_open_fn
            ? v4l2_open_fn(video, O_RDWR | O_NONBLOCK)
            : open(video, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            fprintf(stderr, "oa-ir-blast: %s: %s\n", video, strerror(errno));
            return 1;
        }
        if (setfd)
            setfd(fd);
    }
    if (set_protocol && set)
        (void)set(protocol);

    for (;;) {
        uint32_t code = 0;
        uint8_t aux = 0, flag = 0;
        int rc = rx(&code, &aux, &flag);
        if (rc != 0) {
            usleep(20000);
            continue;
        }
        struct oa_ir_blast_record record = { code, aux, flag };
        if (write_all(STDOUT_FILENO, &record, sizeof record) != 0)
            return 1;
    }
}
