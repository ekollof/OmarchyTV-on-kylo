/* Pure AD82587 register-control tests. No amp, I2C or TV. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/amp_controls.h"

struct fake {
    uint8_t regs[38];
    int writes, reads;
    int fail_write, fail_read, mismatch;
    uint8_t last_reg, last_byte;
};

static int f_write(void *ud, uint8_t reg, uint8_t byte)
{
    struct fake *f = ud;
    f->writes++;
    f->last_reg = reg;
    f->last_byte = byte;
    if (f->fail_write)
        return 0;
    if (reg < sizeof f->regs)
        f->regs[reg] = byte;
    return 1;
}

static int f_read(void *ud, uint8_t reg, uint8_t count, uint8_t *out)
{
    struct fake *f = ud;
    f->reads++;
    f->last_reg = reg;
    if (f->fail_read || count == 0 || reg >= sizeof f->regs)
        return 0;
    for (uint8_t i = 0; i < count; i++)
        out[i] = f->regs[reg + i < sizeof f->regs ? reg + i : sizeof f->regs - 1];
    if (reg == 3 && f->mismatch)
        out[0] ^= 0x01;
    return 1;
}

int main(void)
{
    struct fake f;
    oa_amp_register_io io;
    int value = -1;

    memset(&f, 0, sizeof f);
    io.ud = &f; io.write = f_write; io.read = f_read;

    /* SetVolume: write reg3, read back reg3, require a byte match. */
    assert(oa_ad82587_set_volume(&io, 0x1b) == 1);
    assert(f.last_reg == 3 && f.regs[3] == 0x1b && f.writes == 1 && f.reads == 1);
    assert(oa_ad82587_get_volume(&io, &value) == 1 && value == 0x1b);

    f.fail_write = 1;
    assert(oa_ad82587_set_volume(&io, 10) == 0);
    f.fail_write = 0;
    f.mismatch = 1;
    assert(oa_ad82587_set_volume(&io, 10) == 0);
    f.mismatch = 0;
    f.fail_read = 1;
    assert(oa_ad82587_set_volume(&io, 10) == 0);
    assert(oa_ad82587_get_volume(&io, &value) == 0);
    f.fail_read = 0;

    /* SoftMute: OR 0x0e on, AND 0xf1 off; write only after a successful read. */
    f.regs[2] = 0x20;
    assert(oa_ad82587_soft_mute(&io, 1) == 1);
    assert(f.regs[2] == 0x2e && f.last_reg == 2 && f.last_byte == 0x2e);
    assert(oa_ad82587_soft_mute(&io, 0) == 1);
    assert(f.regs[2] == 0x20 && f.last_byte == 0x20);

    f.fail_read = 1;
    assert(oa_ad82587_soft_mute(&io, 1) == 0);
    f.fail_read = 0;
    f.fail_write = 1;
    assert(oa_ad82587_soft_mute(&io, 1) == 0);
    f.fail_write = 0;

    /* Reject invalid inputs without touching the transports. */
    int w = f.writes, r = f.reads;
    assert(oa_ad82587_set_volume(&io, -1) == 0);
    assert(oa_ad82587_set_volume(&io, 256) == 0);
    assert(oa_ad82587_soft_mute(&io, 2) == 0);
    assert(oa_ad82587_get_volume(&io, NULL) == 0);
    assert(oa_ad82587_set_volume(NULL, 1) == 0);
    assert(f.writes == w && f.reads == r);

    puts("ok amp controls: SetVolume reg3 write/readback, GetVolume and "
         "SoftMute reg2 masks with vendor booleans");
    return 0;
}
