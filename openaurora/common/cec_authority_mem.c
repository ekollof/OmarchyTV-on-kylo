#define _LARGEFILE64_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "openaurora/cec_authority_mem.h"

static int sys_open(void *context, const char *path, int flags)
{
    (void)context;
    return open(path, flags);
}
static void *sys_map(void *context, int fd, uint64_t base, size_t size)
{
    (void)context;
    void *map = mmap64(NULL, size, PROT_READ, MAP_SHARED, fd, (off64_t)base);
    /* mmap failure is MAP_FAILED ((void *)-1), not NULL. Normalize so the
     * opener's NULL check is sufficient on the system path too. */
    return map == MAP_FAILED ? NULL : map;
}
static int sys_unmap(void *context, void *address, size_t size)
{
    (void)context;
    return munmap(address, size);
}
static int sys_close(void *context, int fd)
{
    (void)context;
    return close(fd);
}
static const oa_cec_authority_mem_io system_io = {
    NULL, sys_open, sys_map, sys_unmap, sys_close
};

static int read_mirror(void *context, uint32_t *value)
{
    oa_cec_authority_mem *m = context;
    if (!m || !m->map || !value) {
        errno = EINVAL;
        return -1;
    }
    memcpy(value, (const char *)m->map + OA_CEC_MIRROR_OFFSET,
           sizeof *value);
    return 0;
}

/* The native STBC 0xf011 write is not recovered/linkable from userspace.
 * Returning vendor failure makes a clear authority bit fail closed. */
static int write_register(void *context, uint16_t reg, uint8_t value)
{
    (void)context;
    (void)reg;
    (void)value;
    return 0;
}

int oa_cec_authority_mem_open(oa_cec_authority_mem *m,
                              const oa_cec_authority_mem_io *io)
{
    if (!m) {
        errno = EINVAL;
        return -1;
    }
    if (m->fd != -1 || m->map) {
        errno = EBUSY;
        return -1;
    }
    if (!io) {
#if defined(__arm__) && UINTPTR_MAX == UINT32_MAX
        io = &system_io;
#else
        errno = ENOTSUP;
        return -1;
#endif
    }
    (void)system_io;
    if (!io->open || !io->map || !io->unmap || !io->close) {
        errno = EINVAL;
        return -1;
    }
    m->io = *io;
    int fd = io->open(io->context, "/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0) {
        m->io = (oa_cec_authority_mem_io){ 0 };
        return -1;
    }
    void *map = io->map(io->context, fd, OA_CEC_MIRROR_BASE,
                        OA_CEC_MIRROR_PAGE);
    if (!map || map == MAP_FAILED) {
        int primary = errno ? errno : EIO;
        (void)io->close(io->context, fd);
        m->io = (oa_cec_authority_mem_io){ 0 };
        errno = primary;
        return -1;
    }
    m->fd = fd;
    m->map = map;
    m->authority.context = m;
    m->authority.read_mirror = read_mirror;
    m->authority.write_register = write_register;
    return 0;
}

void oa_cec_authority_mem_close(oa_cec_authority_mem *m)
{
    if (!m)
        return;
    if (m->map)
        (void)m->io.unmap(m->io.context, m->map, OA_CEC_MIRROR_PAGE);
    if (m->fd >= 0)
        (void)m->io.close(m->io.context, m->fd);
    m->fd = -1;
    m->map = NULL;
    m->io = (oa_cec_authority_mem_io){ 0 };
    m->authority = (oa_cec_authority_io){ NULL, NULL, NULL };
}
