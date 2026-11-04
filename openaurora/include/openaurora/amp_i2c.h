#ifndef OPENAURORA_AMP_I2C_H
#define OPENAURORA_AMP_I2C_H
#include <stdint.h>
#include <linux/i2c-dev.h>
#include <sys/stat.h>
typedef struct oa_amp_i2c oa_amp_i2c;
/* Synchronous POSIX-style callbacks: -1/errno on syscall error; transfer
 * returns the completed message count. Table is copied, code/context borrowed
 * through close. No callback may retain request/buffer pointers after return. */
typedef struct {
    void *context;
    int (*open)(void *,const char *,int);
    int (*stat)(void *,int,struct stat *);
    int (*transfer)(void *,int,struct i2c_rdwr_ioctl_data *);
    int (*close)(void *,int);
    void (*delay_ms)(void *,unsigned);
} oa_amp_i2c_io;
/* MB230 AD82587 only: bus3/address34, one-byte register. All APIs return0 or
 * negative errno. Open copies callbacks and validates the opened char device
 * (89:3). NULL io selects native ARM32 syscalls, ENOTSUP elsewhere. allow_write
 * must be0 or1; it is an explicit permission, NOT an exclusive hardware lease.
 * Caller establishes authority, serializes calls and uses a blocking worker.
 * No GPIO, amp init/shutdown, HAL init, frequency change or automatic replay. */
int oa_amp_i2c_open(oa_amp_i2c **out,int allow_write,const oa_amp_i2c_io *io);
/* Fixed firmware-understood03..05 read; output untouched on short/error reply. */
int oa_amp_i2c_read_volume(oa_amp_i2c *session,uint8_t out[3]);
/* Registers0..37 only, exactly one byte,10ms delay after complete transfer.
 * Requires allow_write. Open/read/close perform no register-value writes. */
int oa_amp_i2c_write(oa_amp_i2c *session,unsigned reg,const uint8_t *value);
/* amp_init.io.write adapter: vendor boolean1/0; errno-style diagnostics remain
 * available from error(). Never pass write()'s negative errno as vendor true. */
int oa_amp_i2c_write_cb(void *session,unsigned reg,const uint8_t *value);
int oa_amp_i2c_error(oa_amp_i2c *session);
/* Linux final-close errors consume the fd: handle is cleared/freed even on
 * error, never replayed. All inherited handles reject ECHILD before I/O. */
int oa_amp_i2c_close(oa_amp_i2c **session);
#endif
