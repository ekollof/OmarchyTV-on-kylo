/* Pure model of recovered AD82587 register controls. No device I/O. */
#include "openaurora/amp_controls.h"

#include <stddef.h>

int oa_ad82587_set_volume(const oa_amp_register_io *io, int volume)
{
    uint8_t out = 0;
    uint8_t byte;
    if (!io || !io->read || !io->write || volume < 0 || volume > 255)
        return 0;
    byte = (uint8_t)volume;
    if (!io->write(io->ud, 3, byte))
        return 0;
    if (!io->read(io->ud, 3, 1, &out))
        return 0;
    return out == byte ? 1 : 0;
}

int oa_ad82587_get_volume(const oa_amp_register_io *io, int *out)
{
    uint8_t byte = 0;
    if (!io || !io->read || !out)
        return 0;
    if (!io->read(io->ud, 3, 1, &byte))
        return 0;
    *out = byte;
    return 1;
}

int oa_ad82587_soft_mute(const oa_amp_register_io *io, int on)
{
    uint8_t byte = 0;
    uint8_t next;
    if (!io || !io->read || !io->write || (on != 0 && on != 1))
        return 0;
    if (!io->read(io->ud, 2, 1, &byte))
        return 0;
    next = on ? (uint8_t)(byte | 0x0eu) : (uint8_t)(byte & 0xf1u);
    return io->write(io->ud, 2, next) ? 1 : 0;
}
