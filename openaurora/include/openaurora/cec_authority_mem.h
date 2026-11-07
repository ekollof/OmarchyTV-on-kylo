/* Read-only native CEC authority check.
 *
 * VPL_CEC_Open reads the mirror word at physical 0xfc040200 and, when bit 1
 * is clear, writes STBC 0xf011 with the mirror's low byte plus bit 1. The TV's
 * mirror is mapped through /dev/mem (verified read-only: 0x000001ab, bit 1
 * set). This helper maps only that page and exposes the recovered
 * oa_cec_authority_io contract. It deliberately has no native STBC write path:
 * write_register returns 0, so a clear bit fails closed before the device is
 * opened instead of guessing an IPC/register write.
 *
 * The mapping is read-only and owned by the caller. The interface/context must
 * stay alive through oa_cec_authority_ensure (the owner copies the struct).
 * Injected I/O is for offline fixtures; no device is opened unless requested. */
#ifndef OPENAURORA_CEC_AUTHORITY_MEM_H
#define OPENAURORA_CEC_AUTHORITY_MEM_H
#include <stddef.h>
#include <stdint.h>
#include "openaurora/cec_authority.h"

#define OA_CEC_MIRROR_BASE UINT64_C(0xfc040000)
#define OA_CEC_MIRROR_OFFSET 0x200u
#define OA_CEC_MIRROR_PAGE 4096u

typedef struct {
    void *context;
    int (*open)(void *context, const char *path, int flags);
    /* Map `size` bytes at physical offset `base`; NULL/on failure. */
    void *(*map)(void *context, int fd, uint64_t base, size_t size);
    int (*unmap)(void *context, void *address, size_t size);
    int (*close)(void *context, int fd);
} oa_cec_authority_mem_io;

typedef struct {
    oa_cec_authority_mem_io io;
    int fd;
    void *map;
    oa_cec_authority_io authority;
} oa_cec_authority_mem;

#define OA_CEC_AUTHORITY_MEM_INIT \
    { .fd = -1, .map = NULL, .authority = { NULL, NULL, NULL } }

/* NULL io selects /dev/mem + mmap64 on the target ARM ABI; ENOTSUP elsewhere.
 * 0 on success, -1/errno. On failure no mapping or fd is retained. */
int oa_cec_authority_mem_open(oa_cec_authority_mem *memory,
                              const oa_cec_authority_mem_io *io);
/* Unmaps and closes; safe to call on a partially initialized struct and
 * repeatably. */
void oa_cec_authority_mem_close(oa_cec_authority_mem *memory);

#endif
