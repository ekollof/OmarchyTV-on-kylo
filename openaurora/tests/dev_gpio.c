/* Host regression for the recovered DEV GPIO descriptor/level provider.
 * Injects the /dev/ntgpio ioctl and HAL/STBC level calls; no device access. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/dev_gpio.h"

static int opened, closed, ioctls, reads, writes;
static int last_fd, last_cmd;
static char last_name[64];
static int last_pin, last_level;
static int fail_open, fail_ioctl;

static int open_ntgpio(void *ud)
{
    (void)ud;
    opened++;
    if (fail_open) {
        errno = EACCES;
        return -1;
    }
    return 42;
}

static int close_fd(void *ud, int fd)
{
    (void)ud;
    closed++;
    last_fd = fd;
    return 0;
}

static int ioctl_desc(void *ud, int fd, unsigned long cmd, void *buf)
{
    (void)ud;
    ioctls++;
    last_fd = fd;
    last_cmd = (int)cmd;
    unsigned char *p = buf;
    uintptr_t name;
    memcpy(&name, p + 0x80, sizeof name);
    snprintf(last_name, sizeof last_name, "%s", (const char *)name);
    for (size_t i = 0; i < 0x80; i++)
        assert(p[i] == 0);
    if (fail_ioctl || !strcmp(last_name, "MHL_CDSENSE")) {
        errno = 524;
        return -1;
    }
    memset(buf, 0, 0x88);
    uint32_t pin = !strcmp(last_name, "HDMI_HOTPLUG_1") ? 274 :
                   !strcmp(last_name, "HDMI_HOTPLUG_2") ? 275 : 276;
    memcpy(p + 0x00, &pin, sizeof pin);
    if (!strcmp(last_name, "HDMI_HOTPLUG_1")) {
        memcpy(p + 0x08, "output", 6);
        memcpy(p + 0x20, "low", 3);
        memcpy(p + 0x38, "low", 3);
    } else if (!strcmp(last_name, "HDMI_HOTPLUG_2")) {
        memcpy(p + 0x08, "input", 5);
        memcpy(p + 0x20, "high", 4);
        memcpy(p + 0x38, "high", 4);
    } else {
        memcpy(p + 0x08, "other", 5);
        memcpy(p + 0x20, "mid", 3);
        memcpy(p + 0x38, "other", 5);
    }
    return 0;
}

static int read_level(void *ud, int pin, int *level)
{
    (void)ud;
    reads++;
    last_pin = pin;
    *level = 1;
    return 0;
}

static int write_level(void *ud, int pin, int level)
{
    (void)ud;
    writes++;
    last_pin = pin;
    last_level = level;
    return 0;
}

int main(void)
{
    const struct oa_dev_gpio_io io = {
        .open_ntgpio = open_ntgpio, .close_fd = close_fd,
        .ioctl_desc = ioctl_desc, .read_level = read_level,
        .write_level = write_level
    };
    struct oa_dev_gpio g;
    struct oa_gpio_desc d;

    assert(oa_dev_gpio_open(NULL, &io) < 0);
    assert(oa_dev_gpio_open(&g, NULL) < 0);
    assert(oa_dev_gpio_open(&g, &(struct oa_dev_gpio_io){0}) < 0);

    fail_open = 1;
    assert(oa_dev_gpio_open(&g, &io) == -EACCES);
    fail_open = 0;
    assert(oa_dev_gpio_open(&g, &io) == 0);
    assert(opened == 2 && g.fd == 42 && g.opened);

    assert(oa_dev_gpio_describe(&g, 204, &d) == 1);
    assert(d.id == 204 && d.pin == 274 && d.mode == 2 &&
           d.normal == 0 && d.active == 0);
    assert(ioctls == 1 && last_fd == 42 && last_cmd == (int)0xc088671fUL);
    assert(!strcmp(last_name, "HDMI_HOTPLUG_1"));

    assert(oa_dev_gpio_describe(&g, 205, &d) == 1);
    assert(d.pin == 275 && d.mode == 1 && d.normal == 1 && d.active == 1);
    assert(!strcmp(last_name, "HDMI_HOTPLUG_2"));

    assert(oa_dev_gpio_describe(&g, 206, &d) == 1);
    assert(d.pin == 276 && d.mode == 0 && d.normal == 2 && d.active == 2);
    assert(!strcmp(last_name, "HDMI_HOTPLUG_3"));

    /* The live TV has no MHL_CDSENSE node; the ioctl error is "absent", not
     * a provider failure. */
    assert(oa_dev_gpio_describe(&g, 192, &d) == 0);
    assert(!strcmp(last_name, "MHL_CDSENSE"));
    assert(oa_dev_gpio_describe(&g, 999, &d) == 0);
    assert(ioctls == 4);
    assert(oa_dev_gpio_describe(NULL, 204, &d) == -EINVAL);
    assert(oa_dev_gpio_describe(&g, 204, NULL) == -EINVAL);

    assert(oa_dev_gpio_get_level(&g, 274, &d.pin) == 0);
    assert(reads == 1 && last_pin == 274 && d.pin == 1);
    assert(oa_dev_gpio_set_level(&g, 274, 0) == 0);
    assert(writes == 1 && last_pin == 274 && last_level == 0);
    assert(oa_dev_gpio_set_level(&g, 275, 1) == 0);
    assert(writes == 2 && last_pin == 275 && last_level == 1);

    /* DEV_GPIO_SetLevel rejects level > 1 and pin > 0x11a before any HAL or
     * STBC call. */
    assert(oa_dev_gpio_set_level(&g, 274, 2) == -EINVAL);
    assert(oa_dev_gpio_set_level(&g, 283, 0) == -EINVAL);
    assert(oa_dev_gpio_set_level(&g, -1, 0) == -EINVAL);
    assert(writes == 2);
    assert(oa_dev_gpio_set_level(&g, 282, 0) == 0);
    assert(writes == 3 && last_pin == 282);

    /* Recovered STBC mapping: (pin+0x38)&0xff and 1 -> 2, 0 -> 3. */
    assert(oa_dev_gpio_stbc_channel(274) == 74);
    assert(oa_dev_gpio_stbc_channel(275) == 75);
    assert(oa_dev_gpio_stbc_channel(276) == 76);
    assert(oa_dev_gpio_stbc_channel(277) == 77);
    assert(oa_dev_gpio_stbc_channel(200) == 0);
    assert(oa_dev_gpio_stbc_channel(255) == 55);
    assert(oa_dev_gpio_stbc_channel(256) == 56);
    assert(oa_dev_gpio_stbc_value(0) == 3);
    assert(oa_dev_gpio_stbc_value(1) == 2);
    assert(oa_dev_gpio_stbc_value(2) == 2);

    /* The name table is the shipping au8GPIOFuncName data. */
    assert(!strcmp(oa_dev_gpio_name(192), "MHL_CDSENSE"));
    assert(!strcmp(oa_dev_gpio_name(117), "EARPHONE"));
    assert(!strcmp(oa_dev_gpio_name(193), "HDMI_HOTPLUG_5"));
    assert(!strcmp(oa_dev_gpio_name(204), "HDMI_HOTPLUG_1"));
    assert(!strcmp(oa_dev_gpio_name(207), "HDMI_HOTPLUG_4"));
    assert(!oa_dev_gpio_name(0));

    /* An ioctl failure other than a missing node is also "absent". */
    fail_ioctl = 1;
    assert(oa_dev_gpio_describe(&g, 204, &d) == 0);
    fail_ioctl = 0;

    oa_dev_gpio_close(&g);
    assert(closed == 1 && last_fd == 42 && !g.opened);
    oa_dev_gpio_close(&g);
    assert(closed == 1);

    puts("ok dev gpio: 0x88 name/ioctl descriptor parse (pin/mode/normal/active), "
         "missing MHL node, pin/level validation and STBC channel/value mapping");
    return 0;
}
