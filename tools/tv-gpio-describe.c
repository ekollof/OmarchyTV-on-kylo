/*
 * Read-only runtime GPIO descriptor query for the TV.
 *
 * Issues the recovered DEV_GPIO_GetDescription ioctl (0xc088671f) exactly as
 * aurora 0x00850c38 does: a zeroed 0x88-byte buffer with a pointer to the
 * 24-byte DTS function-name record at +0x80. The kernel looks the name up in
 * func_pin_tbl and copies back the descriptor:
 *
 *   +0x00  pin
 *   +0x04  gpio_fun_pin[2]
 *   +0x08  gpio_mode string  ("input"/"output")
 *   +0x20  normal string     ("low"/"high")
 *   +0x38  active string     ("low"/"high")
 *   +0x50  suspend/idle string
 *   +0x68  int_type string
 *
 * No GPIO claim, direction change, level write, export, global init or
 * ioctl 0x6701/0x6702 is issued. This is a view-only query.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define GPIO_DESC_IOCTL 0xc088671fUL
#define GPIO_DESC_SIZE 0x88u
#define GPIO_NAME_SIZE 24u

struct gpio_id {
    int id;
    const char *name;
};

static const struct gpio_id ids[] = {
    { 192, "MHL_CDSENSE" },
    { 193, "HDMI_HOTPLUG_5" },
    { 204, "HDMI_HOTPLUG_1" },
    { 205, "HDMI_HOTPLUG_2" },
    { 206, "HDMI_HOTPLUG_3" },
    { 207, "HDMI_HOTPLUG_4" },
};

static void field(const unsigned char *buf, size_t off, char out[GPIO_NAME_SIZE + 1])
{
    size_t n = 0;
    while (n < GPIO_NAME_SIZE && buf[off + n]) {
        out[n] = (char)buf[off + n];
        n++;
    }
    out[n] = 0;
}

static const char *level_text(const char *s)
{
    if (!strcmp(s, "low")) return "low(0)";
    if (!strcmp(s, "high")) return "high(1)";
    return "other(2)";
}

int main(void)
{
    int fd = open("/dev/ntgpio", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "open /dev/ntgpio: %s\n", strerror(errno));
        return 1;
    }
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        unsigned char buf[GPIO_DESC_SIZE];
        char name[64];
        char mode[GPIO_NAME_SIZE + 1], normal[GPIO_NAME_SIZE + 1];
        char active[GPIO_NAME_SIZE + 1], idle[GPIO_NAME_SIZE + 1];
        char int_type[GPIO_NAME_SIZE + 1];
        uint32_t ptr = (uint32_t)(uintptr_t)name;
        memset(buf, 0, sizeof buf);
        memset(name, 0, sizeof name);
        strncpy(name, ids[i].name, sizeof name - 1);
        memcpy(buf + 0x80, &ptr, sizeof ptr);

        errno = 0;
        int rc = ioctl(fd, GPIO_DESC_IOCTL, buf);
        int err = errno;
        field(buf, 0x08, mode);
        field(buf, 0x20, normal);
        field(buf, 0x38, active);
        field(buf, 0x50, idle);
        field(buf, 0x68, int_type);
        uint32_t pin, fun2;
        memcpy(&pin, buf + 0x00, sizeof pin);
        memcpy(&fun2, buf + 0x04, sizeof fun2);
        printf("id=%d name=%-14s rc=%d errno=%d pin=%u fun2=%u "
               "mode='%s' normal='%s' active='%s' idle='%s' int_type='%s'\n",
               ids[i].id, ids[i].name, rc, err, pin, fun2,
               mode, normal, active, idle, int_type);
        if (rc == 0)
            printf("  parsed: active=%s normal=%s\n",
                   level_text(active), level_text(normal));
    }
    close(fd);
    return 0;
}
