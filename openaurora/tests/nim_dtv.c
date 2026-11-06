#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/nim_dtv.h"

static int seq[32], nseq;
static int reads[4], writes[4], write_values[4];
static int nreads, nwrites;
static int fail_code = -1, fail_io = -1; /* 0=read, 1=write */
static int module_rc = 1;
static int signal_count;
static int init_rc;

static int module_buf_info(int module, void *ud)
{
    (void)ud;
    assert(module == 9);
    return module_rc;
}
static int init_nim_func(void *ud) { (void)ud; return init_rc; }
static int hal_demod_initialize(void *ud) { (void)ud; return 0; }
static int connect_hal(void *ud) { (void)ud; return 0; }
static int ir_read(int bus, int code, uint8_t *value, void *ud)
{
    (void)ud;
    assert(bus == OA_NIM_DTV_IIC_BUS);
    assert(nreads < 4);
    reads[nreads++] = code;
    *value = 0; /* vendor zeroes the 3-byte stack buffer before code 21 */
    return !(code == fail_code && fail_io == 0);
}
static int ir_write(int bus, int code, uint8_t value, void *ud)
{
    (void)ud;
    assert(bus == OA_NIM_DTV_IIC_BUS);
    assert(nwrites < 4);
    writes[nwrites] = code;
    write_values[nwrites++] = value;
    return !(code == fail_code && fail_io == 1);
}
static void signal_done(void *ud) { (void)ud; signal_count++; }
static void step(int value, void *ud)
{
    (void)ud;
    assert(nseq < 32);
    seq[nseq++] = value;
}

static const oa_nim_dtv_ops ops = {
    module_buf_info, init_nim_func, hal_demod_initialize, connect_hal,
    ir_read, ir_write, signal_done, step
};

static void reset(void)
{
    nseq = nreads = nwrites = signal_count = 0;
    fail_code = -1;
    fail_io = -1;
    module_rc = 1;
    init_rc = 0;
    memset(seq, 0, sizeof seq);
    memset(reads, 0, sizeof reads);
    memset(writes, 0, sizeof writes);
    memset(write_values, 0, sizeof write_values);
}

static void expect_tail(int ir_steps)
{
    int i = nseq - 3;
    assert(seq[i++] == OA_NIM_DTV_STEP_ACTIVE);
    assert(seq[i++] == OA_NIM_DTV_STEP_SIGNAL);
    assert(seq[i] == OA_NIM_DTV_STEP_UNLOCK);
    assert(signal_count == 1);
    (void)ir_steps;
}

int main(void)
{
    for (int m = 0; m <= 3; m++) {
        assert(oa_nim_dtv_model_supported(m) == 1);
        assert(oa_nim_dtv_active_module(m) == 3);
    }
    assert(oa_nim_dtv_model_supported(4) == 0);
    assert(oa_nim_dtv_active_module(4) == 1);
    assert(oa_nim_dtv_model_supported(5) == 0);
    assert(oa_nim_dtv_active_module(5) == 3);
    assert(oa_nim_dtv_model_supported(-1) == 0);
    assert(oa_nim_dtv_active_module(-1) == 3);
    assert(OA_NIM_DTV_TASK_ENTRY == 0x008601adu);
    assert(OA_NIM_DTV_TASK_STACK == 0x1000u);
    assert(OA_NIM_DTV_TASK_PRIORITY == 14);
    assert(OA_NIM_DTV_TERM_ACTIVE == 0xffu);
    assert(OA_NIM_DTV_IIC_BUS == 0 && OA_NIM_DTV_IIC_DEV == 0);
    assert(OA_NIM_DTV_IIC_REG == 0x80 && OA_NIM_DTV_IIC_SIZE == 1 &&
           OA_NIM_DTV_IIC_LEN == 1);

    oa_nim_dtv_result r;
    reset();
    assert(oa_nim_dtv_task_run(0, 1, &ops, NULL, &r) == 0);
    int want[] = {
        OA_NIM_DTV_STEP_LOCK, OA_NIM_DTV_STEP_MODULE_BUF,
        OA_NIM_DTV_STEP_INIT_NIM, OA_NIM_DTV_STEP_HAL_DEMOD,
        OA_NIM_DTV_STEP_CONNECT_HAL, OA_NIM_DTV_STEP_IR21,
        OA_NIM_DTV_STEP_IR17, OA_NIM_DTV_STEP_IR64,
        OA_NIM_DTV_STEP_ACTIVE, OA_NIM_DTV_STEP_SIGNAL,
        OA_NIM_DTV_STEP_UNLOCK
    };
    assert(nseq == (int)(sizeof want / sizeof want[0]));
    for (unsigned i = 0; i < sizeof want / sizeof want[0]; i++)
        assert(seq[i] == want[i]);
    assert(nreads == 3 && reads[0] == 21 && reads[1] == 17 && reads[2] == 64);
    assert(nwrites == 3 && writes[0] == 21 && writes[1] == 17 && writes[2] == 64);
    assert(write_values[0] == 0 && write_values[1] == 8 && write_values[2] == 2);
    assert(r.ir_value[0] == 0 && r.ir_value[1] == 8 && r.ir_value[2] == 2);
    assert(r.module_buf_rc == 1 && r.initialized == 1 && r.active_module == 0xff);
    assert(r.failed_step == -1 && r.failed_io == -1 && r.hal_used == 1);

    reset();
    assert(oa_nim_dtv_task_run(1, 0, &ops, NULL, &r) == 0);
    assert(nseq == 9 && r.hal_used == 0);
    assert(seq[3] == OA_NIM_DTV_STEP_IR21);

    /* Module-buffer and NIM-init failures are logged/ignored by the vendor. */
    reset(); module_rc = 0;
    assert(oa_nim_dtv_task_run(2, 0, &ops, NULL, &r) == 0);
    assert(r.module_buf_rc == 0 && r.initialized == 1 && nreads == 3);
    reset(); init_rc = -1;
    assert(oa_nim_dtv_task_run(3, 0, &ops, NULL, &r) == 0);
    assert(r.initialized == 1 && nreads == 3);

    /* Each IIC read/write rejection stops that code and later codes. */
    static const int codes[] = { 21, 17, 64 };
    static const int step_ids[] = {
        OA_NIM_DTV_STEP_IR21, OA_NIM_DTV_STEP_IR17, OA_NIM_DTV_STEP_IR64
    };
    for (int io = 0; io <= 1; io++) {
        for (int k = 0; k < 3; k++) {
            reset(); fail_code = codes[k]; fail_io = io;
            assert(oa_nim_dtv_task_run(0, 0, &ops, NULL, &r) == 0);
            int ignored_write = io == 1 && k == 2; /* code 64 write unchecked */
            if (ignored_write) {
                assert(r.failed_step == -1 && r.failed_io == -1);
                assert(nreads == 3 && nwrites == 3);
            } else {
                assert(r.failed_step == step_ids[k] && r.failed_io == io);
                assert(nreads == k + 1);
                assert(nwrites == (io == 0 ? k : k + 1));
            }
            assert(r.initialized == 1 && r.active_module == 0xff);
            expect_tail(0);
        }
    }

    assert(oa_nim_dtv_task_run(0, 0, NULL, NULL, &r) == -EINVAL);
    puts("ok nim dtv: model gate/constants and one-shot read/modify/write order, masks and failure finish");
    return 0;
}
