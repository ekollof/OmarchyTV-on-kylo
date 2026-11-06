#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "openaurora/nim_dtv.h"
#include "openaurora/nim_dtv_hal.h"

struct iic_bus { uint32_t bus, reg; };
static int fail_read, fail_write;
static int read_calls, write_calls;
static uint32_t seen_addr;
static uint8_t seen_byte;

static int fake_read(const void *busp, const void *value, unsigned len,
                     void *buf, void *ud)
{
    (void)ud;
    const struct iic_bus *bus = busp;
    assert(bus->bus == 0 && bus->reg == 0x80);
    assert(len == 1 && value && buf);
    read_calls++;
    seen_addr = *(const uint32_t *)value;
    if (fail_read) return -1;
    *(uint8_t *)buf = (uint8_t)(0x5a + seen_addr);
    return 0;
}
static int fake_write(const void *busp, const void *value, unsigned len,
                      void *buf, void *ud)
{
    (void)ud;
    const struct iic_bus *bus = busp;
    assert(bus->bus == 0 && bus->reg == 0x80);
    assert(len == 1 && value && buf);
    write_calls++;
    seen_addr = *(const uint32_t *)value;
    seen_byte = *(const uint8_t *)buf;
    return fail_write ? -1 : 0;
}
static const oa_nim_dtv_iic_ops ops = { fake_read, fake_write };

static int fake_module_buf(int module, void *ud)
{
    assert(module == 9);
    assert(ud != NULL);
    return 1;
}
static int fake_init_nim(void *ud) { assert(ud); return 0; }
static int fake_hal_demod(void *ud) { assert(ud); return 0; }
static int fake_connect(void *ud) { assert(ud); return 0; }
static void fake_signal(void *ud) { assert(ud); }
static void fake_step(int step, void *ud) { (void)step; assert(ud); }

int main(void)
{
    uint8_t value = 0;
    assert(oa_nim_dtv_iic_read(&ops, 0x15, &value, NULL) == 1);
    assert(read_calls == 1 && seen_addr == 0x15 && value == 0x6f);
    fail_read = 1;
    value = 0xaa;
    assert(oa_nim_dtv_iic_read(&ops, 0x11, &value, NULL) == 0);
    assert(value == 0xaa); /* unchanged on failure */
    fail_read = 0;

    assert(oa_nim_dtv_iic_write(&ops, 0x40, 0x02, NULL) == 1);
    assert(write_calls == 1 && seen_addr == 0x40 && seen_byte == 0x02);
    fail_write = 1;
    assert(oa_nim_dtv_iic_write(&ops, 0x40, 0x02, NULL) == 0);

    assert(oa_nim_dtv_iic_read(NULL, 0x15, &value, NULL) == 0);
    assert(oa_nim_dtv_iic_write(NULL, 0x15, 0, NULL) == 0);

    oa_nim_dtv_hal hal = { &ops, NULL };
    value = 0;
    assert(oa_nim_dtv_hal_read(0, 0x15, &value, &hal) == 1 && value == 0x6f);
    assert(oa_nim_dtv_hal_read(1, 0x15, &value, &hal) == 0);
    fail_write = 0;
    assert(oa_nim_dtv_hal_write(0, 0x40, 0x02, &hal) == 1);
    assert(oa_nim_dtv_hal_write(1, 0x40, 0x02, &hal) == 0);

    /* End-to-end: the real HAL adapters drive the one-shot state machine. */
    static const oa_nim_dtv_ops state_ops = {
        fake_module_buf, fake_init_nim, fake_hal_demod, fake_connect,
        oa_nim_dtv_hal_read, oa_nim_dtv_hal_write, fake_signal, fake_step
    };
    oa_nim_dtv_result result;
    fail_read = fail_write = 0;
    assert(oa_nim_dtv_task_run(0, 1, &state_ops, &hal, &result) == 0);
    assert(result.initialized == 1 && result.active_module == 0xff);
    assert(result.ir_value[0] == 0x67 && result.ir_value[1] == 0x6b &&
           result.ir_value[2] == 0x9a);
    puts("ok nim dtv hal: DEV_IIC bus/reg/address packing and Hal success inversion");
    return 0;
}
