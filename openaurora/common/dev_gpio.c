/* Recovered DEV GPIO descriptor and level layer.
 *
 * Source contract: aurora DEV_GPIO_GetDescription 0x00850c38,
 * DEV_GPIO_GetLevel 0x0085145c, DEV_GPIO_SetLevel 0x008516c4; kernel
 * ntgpio ioctl 0x1f at 0x0003d0e0 (see AMP-GPIO-QUERY-2026-11-04).
 *
 * No device access happens here: open/ioctl/read/write are injected. The
 * vendor transport is common/dev_gpio_vendor.c; tests use a fake transport.
 */
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "openaurora/dev_gpio.h"

#define GPIO_DESC_IOCTL 0xc088671fUL
#define GPIO_DESC_SIZE 0x88u
#define GPIO_NAME_SIZE 24u

/* au8GPIOFuncName entries used by the recovered source task, from the
 * shipping aurora.elf .data table at 0x0175f350 (id*24). */
static const struct gpio_name {
    int id;
    const char *name;
} g_names[] = {
    { 192, "MHL_CDSENSE" },
    { 193, "HDMI_HOTPLUG_5" },
    { 204, "HDMI_HOTPLUG_1" },
    { 205, "HDMI_HOTPLUG_2" },
    { 206, "HDMI_HOTPLUG_3" },
    { 207, "HDMI_HOTPLUG_4" },
};

const char *oa_dev_gpio_name(int gpio_id)
{
    for (size_t i = 0; i < sizeof g_names / sizeof g_names[0]; i++)
        if (g_names[i].id == gpio_id)
            return g_names[i].name;
    return NULL;
}

static int parse_level(const unsigned char *p, size_t n)
{
    if (n >= 3 && !memcmp(p, "low", 3)) return 0;
    if (n >= 4 && !memcmp(p, "high", 4)) return 1;
    return 2;
}

static int parse_mode(const unsigned char *p, size_t n)
{
    if (n >= 6 && !memcmp(p, "output", 6)) return 2;
    if (n >= 5 && !memcmp(p, "input", 5)) return 1;
    return 0;
}

int oa_dev_gpio_open(struct oa_dev_gpio *g, const struct oa_dev_gpio_io *io)
{
    if (!g || !io || !io->open_ntgpio || !io->close_fd || !io->ioctl_desc)
        return -EINVAL;
    memset(g, 0, sizeof *g);
    int fd = io->open_ntgpio(io->ud);
    if (fd < 0)
        return errno ? -errno : -EIO;
    g->io = io;
    g->fd = fd;
    g->opened = 1;
    return 0;
}

void oa_dev_gpio_close(struct oa_dev_gpio *g)
{
    if (!g || !g->opened)
        return;
    if (g->io && g->io->close_fd)
        (void)g->io->close_fd(g->io->ud, g->fd);
    g->opened = 0;
    g->fd = -1;
    g->io = NULL;
}

int oa_dev_gpio_describe(struct oa_dev_gpio *g, int gpio_id,
                         struct oa_gpio_desc *out)
{
    if (!g || !out || !g->opened || !g->io || !g->io->ioctl_desc)
        return -EINVAL;
    const char *name = oa_dev_gpio_name(gpio_id);
    if (!name)
        return 0;

    unsigned char buf[GPIO_DESC_SIZE];
    char namebuf[64];
    memset(buf, 0, sizeof buf);
    memset(namebuf, 0, sizeof namebuf);
    size_t n = strlen(name);
    if (n > GPIO_NAME_SIZE)
        n = GPIO_NAME_SIZE;
    memcpy(namebuf, name, n);
    namebuf[n] = 0;
    uintptr_t ptr = (uintptr_t)namebuf;
    memcpy(buf + 0x80, &ptr, sizeof ptr);

    /* DEV_GPIO_GetDescription treats every ioctl failure as "no
     * descriptor"; the caller then logs the missing HDMI definition. */
    if (g->io->ioctl_desc(g->io->ud, g->fd, GPIO_DESC_IOCTL, buf) != 0)
        return 0;

    uint32_t pin;
    memcpy(&pin, buf, sizeof pin);
    memset(out, 0, sizeof *out);
    out->id = gpio_id;
    out->pin = (int)pin;
    out->mode = parse_mode(buf + 0x08, GPIO_NAME_SIZE);
    out->normal = parse_level(buf + 0x20, GPIO_NAME_SIZE);
    out->active = parse_level(buf + 0x38, GPIO_NAME_SIZE);
    return 1;
}

int oa_dev_gpio_get_level(struct oa_dev_gpio *g, int pin, int *level)
{
    if (!g || !level || !g->io || !g->io->read_level)
        return -EINVAL;
    return g->io->read_level(g->io->ud, pin, level);
}

int oa_dev_gpio_set_level(struct oa_dev_gpio *g, int pin, int level)
{
    if (!g || !g->io || !g->io->write_level)
        return -EINVAL;
    /* DEV_GPIO_SetLevel rejects level > 1 and pin > 0x11a before touching
     * the HAL or STBC path. */
    if (level != 0 && level != 1)
        return -EINVAL;
    if (pin < 0 || pin > 0x11a)
        return -EINVAL;
    return g->io->write_level(g->io->ud, pin, level);
}

int oa_dev_gpio_stbc_channel(int pin)
{
    return (pin + 0x38) & 0xff;
}

int oa_dev_gpio_stbc_value(int level)
{
    return level ? 2 : 3;
}
