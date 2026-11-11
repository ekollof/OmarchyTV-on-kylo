#ifndef OPENAURORA_DEV_GPIO_H
#define OPENAURORA_DEV_GPIO_H

/* Recovered DEV GPIO layer used by the cold-start source task.
 *
 * DEV_GPIO_GetDescription (aurora 0x00850c38) sends ioctl 0xc088671f on
 * /dev/ntgpio with a zeroed 0x88-byte buffer and a pointer to the 24-byte DTS
 * function name at +0x80. The kernel looks the name up in func_pin_tbl and
 * copies back:
 *
 *   +0x00 u32 pin
 *   +0x04 u32 gpio_fun_pin[2]
 *   +0x08     gpio_mode string  ("input"/"output")
 *   +0x20     normal string     ("low"/"high")
 *   +0x38     active string     ("low"/"high")
 *   +0x50     idle string
 *   +0x68     int_type string
 *
 * DEV_GPIO_GetLevel (0x0085145c) and DEV_GPIO_SetLevel (0x008516c4) use
 * Hal_Gpio_Read/Write below pin 200 and
 * NDAL_VideoExt_STBC_Get/SetStbcGpio((pin+0x38)&0xff, ...) at or above 200.
 * SetStbcGpio receives 2 for level 1 and 3 for level 0.
 *
 * The transport is injected so the parser and ordering are host-testable
 * without /dev/ntgpio or the vendor HAL.
 */

struct oa_dev_gpio_io {
    void *ud;
    int (*open_ntgpio)(void *ud);
    int (*close_fd)(void *ud, int fd);
    int (*ioctl_desc)(void *ud, int fd, unsigned long cmd, void *buf);
    /* 0 on success, negative errno on failure. */
    int (*read_level)(void *ud, int pin, int *level);
    int (*write_level)(void *ud, int pin, int level);
};

struct oa_dev_gpio {
    const struct oa_dev_gpio_io *io;
    int fd;
    int opened;
};

struct oa_gpio_desc {
    int id;
    int pin;
    int mode;    /* 0 other, 1 input, 2 output */
    int normal;  /* 0 low, 1 high, 2 other */
    int active;  /* 0 low, 1 high, 2 other */
};

int oa_dev_gpio_open(struct oa_dev_gpio *g, const struct oa_dev_gpio_io *io);
void oa_dev_gpio_close(struct oa_dev_gpio *g);

/* 1 descriptor found, 0 absent/ioctl failure (firmware returns NULL), or
 * -EINVAL for bad arguments. The firmware treats every ioctl failure as an
 * absent descriptor, so a missing DTS name is not an error. */
int oa_dev_gpio_describe(struct oa_dev_gpio *g, int gpio_id,
                         struct oa_gpio_desc *out);

int oa_dev_gpio_get_level(struct oa_dev_gpio *g, int pin, int *level);
int oa_dev_gpio_set_level(struct oa_dev_gpio *g, int pin, int level);

/* Recovered STBC mapping: (pin + 0x38) & 0xff and level 1 -> 2, 0 -> 3. */
int oa_dev_gpio_stbc_channel(int pin);
int oa_dev_gpio_stbc_value(int level);

/* Firmware name for a GPIO id, or NULL when this provider has no name. */
const char *oa_dev_gpio_name(int gpio_id);

#endif
