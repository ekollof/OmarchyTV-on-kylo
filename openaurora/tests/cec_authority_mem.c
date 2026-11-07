#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/cec_authority_mem.h"

struct fake {
    int opens, closes, maps, unmaps;
    int open_rc, map_fail;
    int fd_seen;
    uint64_t base;
    size_t size;
    unsigned char page[OA_CEC_MIRROR_PAGE];
};

static int f_open(void *p, const char *path, int flags)
{
    struct fake *f = p;
    assert(!strcmp(path, "/dev/mem"));
    assert(flags == (O_RDONLY | O_SYNC));
    f->opens++;
    if (f->open_rc < 0) { errno = -f->open_rc; return -1; }
    f->fd_seen = 7;
    return 7;
}
static void *f_map(void *p, int fd, uint64_t base, size_t size)
{
    struct fake *f = p;
    assert(fd == 7);
    f->maps++;
    f->base = base;
    f->size = size;
    if (f->map_fail) { errno = ENOMEM; return NULL; }
    return f->page;
}
static int f_unmap(void *p, void *address, size_t size)
{
    struct fake *f = p;
    assert(address == f->page && size == OA_CEC_MIRROR_PAGE);
    f->unmaps++;
    return 0;
}
static int f_close(void *p, int fd)
{
    struct fake *f = p;
    assert(fd == 7);
    f->closes++;
    return 0;
}

static void put32(unsigned char *p, uint32_t v) { memcpy(p, &v, 4); }

int main(void)
{
    struct fake f;
    oa_cec_authority_mem m = OA_CEC_AUTHORITY_MEM_INIT;
    oa_cec_authority_mem_io io = { &f, f_open, f_map, f_unmap, f_close };

    /* Mirror bit 1 set: read-only authority succeeds without a write. */
    memset(&f, 0, sizeof f);
    put32(f.page + OA_CEC_MIRROR_OFFSET, 0x000001abu);
    assert(oa_cec_authority_mem_open(&m, &io) == 0);
    assert(m.fd == 7 && m.map == f.page && m.authority.read_mirror &&
           m.authority.write_register);
    assert(f.opens == 1 && f.maps == 1 && f.base == OA_CEC_MIRROR_BASE &&
           f.size == OA_CEC_MIRROR_PAGE);
    assert(oa_cec_authority_ensure(&m.authority) == 0);
    oa_cec_authority_mem_close(&m);
    assert(f.unmaps == 1 && f.closes == 1 && m.fd == -1 && !m.map);
    oa_cec_authority_mem_close(&m); /* repeatable */
    assert(f.unmaps == 1 && f.closes == 1);

    /* Bit clear: no native write path exists, so authority fails closed. */
    memset(&f, 0, sizeof f);
    put32(f.page + OA_CEC_MIRROR_OFFSET, 0x000001a8u);
    assert(oa_cec_authority_mem_open(&m, &io) == 0);
    errno = 0;
    assert(oa_cec_authority_ensure(&m.authority) == -1);
    assert(errno == EIO);
    oa_cec_authority_mem_close(&m);

    /* Mapping failure closes the fd and retains nothing. */
    memset(&f, 0, sizeof f);
    f.map_fail = 1;
    errno = 0;
    assert(oa_cec_authority_mem_open(&m, &io) == -1);
    assert(errno == ENOMEM && f.closes == 1 && m.fd == -1 && !m.map);

    memset(&f, 0, sizeof f);
    f.open_rc = -EACCES;
    assert(oa_cec_authority_mem_open(&m, &io) == -1);
    assert(errno == EACCES && m.fd == -1 && !m.map);

    /* Invalid/short tables and busy state. */
    assert(oa_cec_authority_mem_open(NULL, &io) == -1 && errno == EINVAL);
    oa_cec_authority_mem_io empty = { &f, NULL, NULL, NULL, NULL };
    assert(oa_cec_authority_mem_open(&m, &empty) == -1 && errno == EINVAL);
    assert(oa_cec_authority_ensure(NULL) == -1 && errno == EINVAL);
#if !defined(__arm__) || UINTPTR_MAX != UINT32_MAX
    assert(oa_cec_authority_mem_open(&m, NULL) == -1 && errno == ENOTSUP);
#endif

    puts("ok cec authority mem: read-only mirror page, bit1 gate, clear-bit "
         "fail-closed, map/fd unwind");
    return 0;
}
