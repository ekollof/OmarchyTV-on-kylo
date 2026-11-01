/* One firmware-understood AD82587 register read on the MB230 target.
 * No discovery scan, GPIO/amp reset, gain/mute write or HAL initialization.
 * See AUDIO-BOARD-2026-11-01 for board identity and PreInit transfer semantics.
 * Native kernel I2C_RDWR serializes this transaction with the stock owner.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static void describe(void)
{
    puts("AD82587_D: /dev/i2c-3, slave 0x34 (vendor 0x68), registers 0x03..0x05 read only");
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--describe")) { describe(); return 0; }
    if (argc != 2 || strcmp(argv[1], "--read-volume")) {
        fprintf(stderr, "usage: %s --describe | --read-volume\n", argv[0]); return 2;
    }
#if !defined(__arm__) || UINTPTR_MAX != UINT32_MAX
    fputs("hardware read requires target ARM32 ABI\n", stderr); return 1;
#else
    _Static_assert(sizeof(struct i2c_msg) == 12, "target I2C message ABI");
    _Static_assert(sizeof(struct i2c_rdwr_ioctl_data) == 8, "target I2C list ABI");
    struct stat node;
    if (stat("/dev/i2c-3", &node)) { perror("stat i2c-3"); return 1; }
    if (!S_ISCHR(node.st_mode) || major(node.st_rdev) != 89 || minor(node.st_rdev) != 3) {
        fputs("refusing unexpected i2c-3 device identity\n", stderr); return 1;
    }
    int fd = open("/dev/i2c-3", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("open i2c-3"); return 1; }
    uint8_t reg = 3, bytes[3] = {0};
    /* Match _i2c_RdWr_Transfer's two-message read. PreInit uses the first
     * message as subaddress irrespective of its flags, and the second flag
     * chooses read/write. Both flags are RD in the vendor helper. */
    struct i2c_msg messages[2] = {
        { .addr = 0x34, .flags = I2C_M_RD, .len = 1, .buf = &reg },
        { .addr = 0x34, .flags = I2C_M_RD, .len = sizeof bytes, .buf = bytes }
    };
    struct i2c_rdwr_ioctl_data request = { .msgs = messages, .nmsgs = 2 };
    int count = ioctl(fd, I2C_RDWR, &request);
    int saved = errno;
    int closed = close(fd);
    if (count < 0) { errno = saved; perror("AD82587 read"); return 1; }
    /* A disabled bus can return zero; HAL's nonnegative check loses that fact. */
    if (count != 2) {
        fprintf(stderr, "incomplete AD82587 read: %d of 2 messages\n", count); return 1;
    }
    if (closed) { perror("close i2c-3"); return 1; }
    describe();
    printf("completed=%d register03=%02x register04=%02x register05=%02x\n",
           count, bytes[0], bytes[1], bytes[2]);
    return 0;
#endif
}
