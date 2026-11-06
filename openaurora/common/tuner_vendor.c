#include "openaurora/tuner_vendor.h"

#ifndef OA_NO_VENDOR

#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "openaurora/lnb_rt5047.h"
#include "openaurora/mxl661_dev.h"

/* libNDAL3 exports. Hal_Iic_Read/Write return 0 on success;
 * HAL_DEMOD_EXT_DVIF_SetTunerType returns 0 on success. */
extern int Hal_Iic_Write(const void *ctx, const void *value, unsigned len,
                         const void *buf);
extern int Hal_Iic_Read(const void *ctx, const void *value, unsigned len,
                        void *buf);
extern int HAL_Demod_Init(int index);
extern int HAL_dvif_open(void);
extern int HAL_DEMOD_EXT_DVIF_SetTunerType(int handle, int type);

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

int oa_tuner_vendor_mxl661_init(uint8_t addr, const oa_mxl661_config *cfg)
{
    oa_mxl661_ops ops = { oa_mxl661_dev_write, oa_mxl661_dev_read };
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

#endif /* !OA_NO_VENDOR */
