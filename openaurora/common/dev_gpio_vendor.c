/* Vendor transport for the recovered DEV GPIO layer.
 *
 * DEV_GPIO_GetLevel (0x0085145c):
 *   pin < 200  -> Hal_Gpio_Read(pin, &level), rc == 0 is success
 *   pin >= 200 -> PLAT_SYS_GetStbcGpio((pin+0x38)&0xff, &level)
 * DEV_GPIO_SetLevel (0x008516c4):
 *   pin < 200  -> Hal_Gpio_Write(pin, level)
 *   pin >= 200 -> PLAT_SYS_SetStbcGpio((pin+0x38)&0xff, level ? 2 : 3)
 *
 * PLAT_SYS_* call NDAL_VideoExt_STBC_* and treat a nonzero result as success.
 * The STBC fd is opened by the platform prelude (NDAL_VideoExt_OpenDevice),
 * so this transport must run under the plat owner.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "openaurora/dev_gpio.h"

#ifdef OA_NO_VENDOR

const struct oa_dev_gpio_io *oa_dev_gpio_vendor_io(void)
{
    return NULL;
}

#else

/* libNDAL3. Both return 0 on success. */
extern int Hal_Gpio_Read(int pin, int *level);
extern int Hal_Gpio_Write(int pin, int level);

/* libNDAL_EX. Get takes value-out and status-out; both return the V4L2 send
 * result, nonzero on success. */
extern int NDAL_VideoExt_STBC_GetStbcGpio(unsigned char pin, uint32_t *level,
                                          uint32_t *status);
extern int NDAL_VideoExt_STBC_SetStbcGpio(unsigned char pin,
                                          unsigned char level);

static int v_open(void *ud)
{
    (void)ud;
    return open("/dev/ntgpio", O_RDWR | O_CLOEXEC);
}

static int v_close(void *ud, int fd)
{
    (void)ud;
    return close(fd);
}

static int v_ioctl(void *ud, int fd, unsigned long cmd, void *buf)
{
    (void)ud;
    return ioctl(fd, cmd, buf);
}

static int v_read(void *ud, int pin, int *level)
{
    (void)ud;
    if (pin < 200)
        return Hal_Gpio_Read(pin, level) == 0 ? 0 : -EIO;
    uint32_t value = 0, status = 0;
    int rc = NDAL_VideoExt_STBC_GetStbcGpio(
        (unsigned char)oa_dev_gpio_stbc_channel(pin), &value, &status);
    *level = (int)(value & 0xffu);
    return rc != 0 ? 0 : -EIO;
}

static int v_write(void *ud, int pin, int level)
{
    (void)ud;
    if (pin < 200)
        return Hal_Gpio_Write(pin, level) == 0 ? 0 : -EIO;
    int rc = NDAL_VideoExt_STBC_SetStbcGpio(
        (unsigned char)oa_dev_gpio_stbc_channel(pin),
        (unsigned char)oa_dev_gpio_stbc_value(level));
    return rc != 0 ? 0 : -EIO;
}

static const struct oa_dev_gpio_io g_vendor_io = {
    .ud = NULL,
    .open_ntgpio = v_open,
    .close_fd = v_close,
    .ioctl_desc = v_ioctl,
    .read_level = v_read,
    .write_level = v_write,
};

const struct oa_dev_gpio_io *oa_dev_gpio_vendor_io(void)
{
    return &g_vendor_io;
}

#endif
