#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/hal_iic.h"
#include "openaurora/tuner_hal.h"

static uint8_t desc[4][0x20];
static unsigned desc_calls;
static int null_desc[4];
static unsigned read_calls;
static unsigned write_calls;
static int rc;

static const uint8_t *get_desc(int index, void *ud)
{
    (void)ud;
    assert(index >= 0 && index < 4);
    desc_calls++;
    return null_desc[index] ? NULL : desc[index];
}

static int fake_read(const void *ctx, const void *value, unsigned len,
                     void *buf, void *ud)
{
    (void)ud;
    const oa_hal_iic_ctx *c = ctx;
    assert(c->bus == 1 && c->addr == 0xc0 && c->subaddr_len == 0);
    assert(c->reserved0 == 0 && c->timeout == OA_TUNER_IIC_TIMEOUT &&
           c->reserved1 == 0);
    assert(len == 3 && value && ((const uint8_t *)value)[0] == 0);
    read_calls++;
    if (rc != 0)
        return rc;
    memcpy(buf, "\x11\x22\x33", 3);
    return 0;
}

static int fake_write(const void *ctx, const void *value, unsigned len,
                      void *buf, void *ud)
{
    (void)ud;
    const oa_hal_iic_ctx *c = ctx;
    assert(c->bus == 1 && c->addr == 0xc0 && c->subaddr_len == 0);
    assert(c->reserved0 == 0 && c->timeout == OA_TUNER_IIC_TIMEOUT &&
           c->reserved1 == 0);
    assert(len == 3 && value && ((const uint8_t *)value)[0] == 0);
    assert(buf && memcmp(buf, "\x07\x08\x09", 3) == 0);
    write_calls++;
    return rc;
}

static const oa_tuner_iic_ops ops = { fake_read, fake_write };

static void test_bus_ids(void)
{
    oa_tuner_buses b = OA_TUNER_BUSES_INIT;
    desc[0][0x19] = 0x21;
    desc[1][0x19] = 0x22;
    desc[2][0x19] = 0x23;
    desc[3][0x19] = 0x24;
    desc_calls = 0;
    assert(oa_tuner_bus_ids(&b, get_desc, NULL) == 0);
    assert(desc_calls == 4);
    assert(b.tuner == 0x21 && b.sat == 0x22 && b.ext_demod == 0x23 &&
           b.lnb == 0x24);

    /* Already resolved entries are not queried again. */
    desc_calls = 0;
    assert(oa_tuner_bus_ids(&b, get_desc, NULL) == 0);
    assert(desc_calls == 0);

    /* Only 0xff entries are resolved; a missing description means bus 0. */
    b.tuner = 0x31;
    b.sat = OA_TUNER_BUS_UNRESOLVED;
    b.ext_demod = 0x33;
    b.lnb = OA_TUNER_BUS_UNRESOLVED;
    null_desc[1] = 1;
    desc_calls = 0;
    assert(oa_tuner_bus_ids(&b, get_desc, NULL) == 0);
    assert(desc_calls == 2);
    assert(b.tuner == 0x31 && b.sat == 0 && b.ext_demod == 0x33 &&
           b.lnb == 0x24);

    /* No callback at all: every unresolved bus falls back to 0. */
    b = (oa_tuner_buses)OA_TUNER_BUSES_INIT;
    assert(oa_tuner_bus_ids(&b, NULL, NULL) == 0);
    assert(b.tuner == 0 && b.sat == 0 && b.ext_demod == 0 && b.lnb == 0);
    assert(oa_tuner_bus_ids(NULL, get_desc, NULL) == -1);
}

static void test_read_write_base(void)
{
    uint8_t buf[3] = { 0 };
    rc = 0;
    read_calls = 0;
    assert(oa_tuner_read_base(&ops, NULL, 1, 0xc0, buf, 3) == 1);
    assert(read_calls == 1 && buf[0] == 0x11 && buf[1] == 0x22 &&
           buf[2] == 0x33);

    rc = -5;
    read_calls = 0;
    assert(oa_tuner_read_base(&ops, NULL, 1, 0xc0, buf, 3) == 0);
    assert(read_calls == 1);

    rc = 0;
    write_calls = 0;
    const uint8_t out[3] = { 0x07, 0x08, 0x09 };
    assert(oa_tuner_write_base(&ops, NULL, 1, 0xc0, out, 3) == 1);
    assert(write_calls == 1);

    rc = -1;
    assert(oa_tuner_write_base(&ops, NULL, 1, 0xc0, out, 3) == 0);

    assert(oa_tuner_read_base(NULL, NULL, 1, 0xc0, buf, 3) == 0);
    assert(oa_tuner_write_base(NULL, NULL, 1, 0xc0, out, 3) == 0);
    assert(oa_tuner_read_base(&ops, NULL, 1, 0xc0, NULL, 3) == 0);
    assert(oa_tuner_write_base(&ops, NULL, 1, 0xc0, NULL, 3) == 0);
}

int main(void)
{
    test_bus_ids();
    test_read_write_base();
    puts("ok tuner hal: four bus IDs from description+0x19, 0xff-only "
         "resolution, ctx {bus,reg,0,0,400,0} and Hal rc inversion");
    return 0;
}
