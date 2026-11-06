#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/hal_iic.h"
#include "openaurora/mxl661.h"
#include "openaurora/mxl661_dev.h"

struct ev {
    int read;
    oa_hal_iic_ctx ctx;
    uint8_t sub;
    uint8_t data;
};

struct fake {
    struct ev ev[256];
    unsigned n;
    uint8_t regs[256];
    uint8_t ptr;
    unsigned writes;
    unsigned reads;
    int fail_write;
    int fail_read;
};

static int hal_write(const void *ctx, const void *value, unsigned len,
                     const void *buf, void *ud)
{
    struct fake *f = ud;
    assert(len == 1 && value && buf);
    f->ev[f->n++] = (struct ev){ 0, *(const oa_hal_iic_ctx *)ctx,
                                 *(const uint8_t *)value,
                                 *(const uint8_t *)buf };
    f->writes++;
    if (f->fail_write == (int)f->writes)
        return -1;
    uint8_t sub = *(const uint8_t *)value;
    uint8_t data = *(const uint8_t *)buf;
    if (sub == 0xfb)
        f->ptr = data;
    else
        f->regs[sub] = data;
    return 0;
}

static int hal_read(const void *ctx, const void *value, unsigned len,
                    void *buf, void *ud)
{
    struct fake *f = ud;
    assert(len == 1 && value && buf);
    f->ev[f->n++] = (struct ev){ 1, *(const oa_hal_iic_ctx *)ctx,
                                 *(const uint8_t *)value, f->regs[f->ptr] };
    f->reads++;
    if (f->fail_read == (int)f->reads)
        return -1;
    *(uint8_t *)buf = f->regs[f->ptr];
    return 0;
}

static void check_ctx(const oa_hal_iic_ctx *c, uint32_t subaddr_len)
{
    assert(c->bus == 0 && c->addr == 0xc0 && c->subaddr_len == subaddr_len);
    assert(c->reserved0 == 0 && c->timeout == OA_HAL_IIC_TIMEOUT_TUNER &&
           c->reserved1 == 0);
}

static void test_write(void)
{
    struct fake f = { 0 };
    oa_mxl661_hal_ops h = { hal_write, hal_read, &f };
    assert(oa_mxl661_dev_write(&h, 0xc0, 0x14, 0x13) == 0);
    assert(f.n == 1 && !f.ev[0].read);
    check_ctx(&f.ev[0].ctx, 1);
    assert(f.ev[0].sub == 0x14 && f.ev[0].data == 0x13);
    assert(f.regs[0x14] == 0x13);
}

static void test_read(void)
{
    struct fake f = { 0 };
    oa_mxl661_hal_ops h = { hal_write, hal_read, &f };
    f.regs[0x20] = 0x5a;
    uint8_t v = 0;
    assert(oa_mxl661_dev_read(&h, 0xc0, 0x20, &v) == 0);
    assert(f.n == 2);
    assert(!f.ev[0].read);
    check_ctx(&f.ev[0].ctx, 1);
    assert(f.ev[0].sub == 0xfb && f.ev[0].data == 0x20);
    assert(f.ev[1].read);
    check_ctx(&f.ev[1].ctx, 0);
    assert(f.ev[1].sub == 0x00 && f.ev[1].data == 0x5a);
    assert(v == 0x5a);
}

static void test_failures(void)
{
    struct fake f = { 0 };
    oa_mxl661_hal_ops h = { hal_write, hal_read, &f };
    uint8_t v = 0;

    f.fail_write = 1;
    assert(oa_mxl661_dev_read(&h, 0xc0, 0x20, &v) == -1);
    assert(f.n == 1); /* the 0xfb write failed; no read followed */

    memset(&f, 0, sizeof f);
    f.fail_read = 1;
    assert(oa_mxl661_dev_read(&h, 0xc0, 0x20, &v) == -1);
    assert(f.n == 2);

    assert(oa_mxl661_dev_read(&h, 0xc0, 0x20, NULL) == -1);
    assert(oa_mxl661_dev_write(NULL, 0xc0, 0, 0) == -1);
    assert(oa_mxl661_dev_read(NULL, 0xc0, 0, &v) == -1);
}

static void test_with_init(void)
{
    struct fake f = { 0 };
    oa_mxl661_hal_ops h = { hal_write, hal_read, &f };
    oa_mxl661_ops ops = { oa_mxl661_dev_write, oa_mxl661_dev_read };
    assert(oa_mxl661_init(&ops, &h, 0xc0, &oa_mxl661_default_config) == 0);
    assert(f.regs[0x31] == 0xd0);
    assert(f.regs[0x04] == 0x06);
    assert(f.regs[0x05] == 0x1b);
    assert(f.regs[0x08] == 0x05);
    assert(f.regs[0x09] == 0x42);
    assert(f.regs[0x5e] == 0x00);
    assert(f.regs[0x0b] == 0x01);
    assert(f.writes == 46 && f.reads == 5); /* 41 logical writes + five 0xfb */
}

int main(void)
{
    test_write();
    test_read();
    test_failures();
    test_with_init();
    puts("ok mxl661 dev: DEV_IIC context, 0xfb pointer read, failure "
         "propagation and full init over Hal_Iic");
    return 0;
}
