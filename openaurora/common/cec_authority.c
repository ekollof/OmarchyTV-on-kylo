#include <errno.h>
#include "openaurora/cec_authority.h"

int oa_cec_authority_ensure(const oa_cec_authority_io *io)
{
    if (!io || !io->read_mirror || !io->write_register) {
        errno = EINVAL;
        return -1;
    }
    uint32_t mirror = 0;
    if (io->read_mirror(io->context, &mirror) != 0) {
        if (!errno) errno = EIO;
        return -1;
    }
    if (mirror & OA_CEC_AUTHORITY_BIT) return 0;
    uint8_t value = (uint8_t)((mirror & 0xffu) | OA_CEC_AUTHORITY_BIT);
    if (io->write_register(io->context, OA_CEC_AUTHORITY_REGISTER, value) != 1) {
        errno = EIO;
        return -1;
    }
    return 0;
}
