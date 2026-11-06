#include "openaurora/tuner_vendor.h"

#ifndef OA_NO_VENDOR

#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "openaurora/lnb_rt5047.h"
#include "openaurora/mxl661_atv.h"
#include "openaurora/mxl661_dev.h"
#include "openaurora/mxl661_rf.h"
#include "openaurora/nim_tune.h"

/* libNDAL3 exports. Hal_Iic_Read/Write return 0 on success;
 * HAL_DEMOD_EXT_DVIF_SetTunerType returns 0 on success. */
extern void Hal_Iic_Init(void);
extern int Hal_Iic_Write(const void *ctx, const void *value, unsigned len,
                         const void *buf);
extern int Hal_Iic_Read(const void *ctx, const void *value, unsigned len,
                        void *buf);
extern int HAL_Demod_Init(int index);
extern int HAL_dvif_open(void);
extern int HAL_DEMOD_EXT_DVIF_SetTunerType(int handle, int type);
/* Linkable NDAL equivalent of the Aurora-only HAL_Demod_User_SetChannel
 * tail. 0 = success. */
extern int HAL_Demod_Set(int frontend, const void *params);

static int hal_write(const void *ctx, const void *value, unsigned len,
                     const void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Write(ctx, value, len, buf);
}

static int hal_read(const void *ctx, const void *value, unsigned len,
                    void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Read(ctx, value, len, buf);
}

static const oa_mxl661_hal_ops hal_ops = { hal_write, hal_read, NULL };
static int g_iic_ready;

void oa_tuner_vendor_iic_init(void)
{
    if (g_iic_ready)
        return;
    Hal_Iic_Init();
    g_iic_ready = 1;
}



int oa_tuner_vendor_mxl661_init(uint8_t addr, const oa_mxl661_config *cfg)
{
    oa_mxl661_ops ops = { oa_mxl661_dev_write, oa_mxl661_dev_read, NULL };
    return oa_mxl661_init(&ops, (void *)&hal_ops, addr, cfg);
}

int oa_tuner_vendor_demod_init(void *ud, int index)
{
    (void)ud;
    return HAL_Demod_Init(index);
}

int oa_tuner_vendor_dvif_open(void *ud)
{
    (void)ud;
    return HAL_dvif_open();
}

int oa_tuner_vendor_set_tuner_type(void *ud, int handle, uint32_t type)
{
    (void)ud;
    return HAL_DEMOD_EXT_DVIF_SetTunerType(handle, (int)type);
}

int oa_tuner_vendor_av2018_response(void *ud)
{
    (void)ud;
    /* C_SalTunerAV2018::GetTunerResponse @0x0084d8cc is `return 1`. */
    return 1;
}

/* Novatek GPIO descriptor ioctl used by DEV_GPIO_GetDescription @0x00850c38.
 * The 0x88-byte input/output record carries the 24-byte function name at
 * +0x80; the driver returns the pin at +0x00 and "low"/"high" level strings
 * at +0x20. */
struct ntk_gpio_desc {
    uint32_t pin;
    uint32_t field1;
    char direction[24];
    char level[24];
    char input_level[72];
    const char *name;
    uint32_t pad;
};

#define NTK_GPIO_DESC _IOWR('g', 0x1f, struct ntk_gpio_desc)

_Static_assert(sizeof(struct ntk_gpio_desc) == 0x88,
               "unexpected Novatek GPIO descriptor size");
_Static_assert(NTK_GPIO_DESC == 0xc088671fu,
               "unexpected Novatek GPIO descriptor ioctl");

extern int Hal_Gpio_Init(void);
extern int Hal_Gpio_Write(int pin, int value);

static int lnb_get_en(void *ud, uint32_t *pin, uint32_t *level)
{
    (void)ud;
    int fd = open("/dev/ntgpio", O_RDWR);
    if (fd < 0)
        return -1;
    static const char name[24] = "LNB_EN";
    struct ntk_gpio_desc desc;
    memset(&desc, 0, sizeof desc);
    desc.name = name;
    int rc = ioctl(fd, NTK_GPIO_DESC, &desc);
    close(fd);
    if (rc < 0)
        return 0; /* no LNB_EN description on this board/profile */
    *pin = desc.pin;
    if (!strncmp(desc.level, "high", 4))
        *level = 1;
    else if (!strncmp(desc.level, "low", 3))
        *level = 0;
    else
        return 0;
    return 1;
}

static int lnb_set_level(void *ud, uint32_t pin, uint32_t level)
{
    (void)ud;
    if (level > 1)
        return -1;
    int rc = Hal_Gpio_Init();
    if (rc != 0)
        return -1;
    return Hal_Gpio_Write((int)pin, (int)level) == 0 ? 0 : -1;
}

int oa_tuner_vendor_lnb_init(void *ud)
{
    (void)ud;
    const oa_rt5047_ops ops = { lnb_get_en, lnb_set_level };
    return oa_rt5047_init(&ops, NULL);
}

int oa_tuner_vendor_hal_iic_read(const void *ctx, const void *value,
                                 unsigned len, void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Read(ctx, value, len, buf);
}

int oa_tuner_vendor_hal_iic_write(const void *ctx, const void *value,
                                  unsigned len, void *buf, void *ud)
{
    (void)ud;
    return Hal_Iic_Write(ctx, value, len, buf);
}

extern int HAL_fe_open(int adapter, int index);
extern int HAL_fe_close(int fe);
extern int HAL_DEMOD_EXT_DeliverConfig(int fe, uint32_t key, const void *data,
                                       int len);
extern int HAL_DEMOD_EXT_SetStandy(int fe, int on);
extern int HAL_Demod_GetStatus(int handle, int index, uint32_t mask, void *buf);

static struct oa_tuner_vendor_tune_ud g_tune_ud = { 0xd, 0 };

static int tune_switch(void *ud, int type)
{
    (void)ud;
    (void)type; /* SAL_DEMOD_TUN_SwhitchTunerType is not exported */
    return 0;
}

static int tune_type(void *ud)
{
    (void)ud;
    return g_tune_ud.tuner_type;
}

static int tune_deliver(void *ud, uint32_t key, const void *data, int len)
{
    (void)ud;
    int index = g_tune_ud.fe_index;
    int fe = HAL_fe_open(0, index);
    if (fe < 0)
        return -1;
    int rc = HAL_DEMOD_EXT_DeliverConfig(fe, key, data, len);
    HAL_fe_close(fe);
    return rc == 0 ? 0 : -1;
}

static int tune_fe_open(void *ud, int index)
{
    (void)ud;
    return HAL_fe_open(0, index);
}

static void tune_fe_close(void *ud, int fe)
{
    (void)ud;
    HAL_fe_close(fe);
}

static int tune_standby(void *ud, int fe, int on)
{
    (void)ud;
    return HAL_DEMOD_EXT_SetStandy(fe, on);
}

static int tune_status(void *ud, int handle, int index, uint32_t mask,
                       void *buf)
{
    (void)ud;
    return HAL_Demod_GetStatus(handle, index, mask, buf);
}

static int tune_rssi(void *ud, int type, uint8_t *rssi)
{
    (void)ud;
    (void)type;
    (void)rssi;
    return 0; /* MxL661 tuner RSSI sequence is not recovered */
}

static void tune_sleep(void *ud, unsigned ms)
{
    (void)ud;
    usleep((useconds_t)ms * 1000u);
}

static int tune_program(void *ud, const struct oa_nim_tune_channel *ch)
{
    (void)ud;
    oa_mxl661_ops mops = { oa_mxl661_dev_write, oa_mxl661_dev_read, tune_sleep };
    struct oa_mxl661_rf_params p = {
        ch->freq_hz, ch->standard, ch->bandwidth, ch->if_khz, 0
    };
    return oa_mxl661_tune_dvbt(&mops, (void *)&hal_ops, 0xc0, &p) == 0 ? 0 : -1;
}

/* Linkable reproduction of the Aurora-only HAL_Demod_User_SetChannel body for
 * DVB-T (signal type 2):
 *   1. FUN_0086aab0's SAL_DEMOD_TUN_ProgramTuner call, using our recovered
 *      MxL661 DVB-T sequence (the record's standard type is 2 -> SAL 6 ->
 *      MxL661 standard 8);
 *   2. the same 0x54-byte HAL_Demod_Set parameter block built by FUN_0086aab0
 *      for its DVB-T/default case.
 * HAL_Demod_User_Initialize and HAL_Demod_User_SetChannel themselves live
 * only in aurora.elf (not libNDAL3/EX), so a replacement process cannot link
 * them; every linkable input they consume is reproduced here. */
static int tune_set_channel(void *ud, int signal_type,
                            const struct oa_nim_raw_channel *record)
{
    (void)ud;
    if (signal_type != 2)
        return -1;
    struct oa_nim_dvbt_setup setup;
    if (oa_nim_raw_channel_setup_dvbt(record, &setup) != 0)
        return -1;

    oa_tuner_vendor_iic_init();
    oa_mxl661_ops mops = { oa_mxl661_dev_write, oa_mxl661_dev_read, tune_sleep };
    struct oa_mxl661_rf_params p = {
        .freq_hz = setup.freq_hz,
        .standard = setup.standard,
        .bandwidth = setup.bandwidth,
        .if_khz = setup.if_khz,
        .start = 0
    };
    if (oa_mxl661_tune_dvbt(&mops, (void *)&hal_ops, 0xc0, &p) != 0)
        return -1;
    return HAL_Demod_Set(0, setup.demod) == 0 ? 0 : -1;
}

static int tune_program_atv(void *ud,
                            const struct oa_mxl661_analog_params *params,
                            const struct oa_mxl661_atv_tables *tables)
{
    (void)ud;
    if (!params || !tables)
        return -1;
    oa_mxl661_ops mops = { oa_mxl661_dev_write, oa_mxl661_dev_read, tune_sleep };
    return oa_mxl661_tune_analog(&mops, (void *)&hal_ops, 0xc0, params,
                                 tables) == 0 ? 0 : -1;
}

static int tune_atv_status(void *ud, int path, uint8_t *locked)
{
    (void)ud;
    if (!locked || (path != 1 && path != 2 && path != 3))
        return -1;
    uint8_t reg = path == 3 ? 0x2c : 0x2b;
    uint8_t value = 0;
    if (oa_mxl661_dev_read((void *)&hal_ops, 0xc0, reg, &value) != 0)
        return -1;
    return oa_mxl661_atv_status_lock(path, value, locked) ? 0 : -1;
}

static const struct oa_nim_tune_ops vendor_tune_ops = {
    tune_switch, tune_type, tune_deliver, tune_fe_open, tune_fe_close,
    tune_standby, tune_status, tune_rssi, tune_program, tune_set_channel,
    tune_program_atv, tune_atv_status
};

const struct oa_nim_tune_ops *oa_tuner_vendor_tune_ops(void)
{
    return &vendor_tune_ops;
}

#endif /* !OA_NO_VENDOR */
