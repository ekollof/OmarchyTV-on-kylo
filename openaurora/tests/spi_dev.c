/* Fake-device test for the recovered DEV_SPI transport. No /dev/ntkspi is
 * opened: open/close/ioctl resolve to this executable's wrappers. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "openaurora/spi_dev.h"

#define FAKE_DEVICE "/dev/oa-spi-fixture"
static int fail_ioctl;
static int fake_closes;
static unsigned ioctl_calls;

int __real_open(const char *path, int flags, ...);
int __wrap_open(const char *path, int flags, ...)
{
    va_list ap;
    va_start(ap, flags);
    int mode = va_arg(ap, int);
    va_end(ap);
    if (!strcmp(path, FAKE_DEVICE)) return 123;
    return __real_open(path, flags, mode);
}
int __real_close(int fd);
int __wrap_close(int fd)
{
    if (fd == 123) { fake_closes++; return 0; }
    return __real_close(fd);
}
int __wrap_ioctl(int fd, unsigned long request, ...)
{
    assert(fd == 123);
    assert(request == OA_SPI_READ_IOCTL);
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    /* 24-byte ARM request: offset,size,pointer,three reserved words. */
    struct {
        uint32_t offset, size;
        uintptr_t buf;
        uint32_t reserved[3];
    } *req = arg;
    assert(req->offset == 0x20 && req->size == 8);
    assert(req->reserved[0] == 0 && req->reserved[1] == 0 && req->reserved[2] == 0);
    ioctl_calls++;
    if (fail_ioctl) { errno = EIO; return -1; }
    unsigned char *out = (unsigned char *)(uintptr_t)req->buf;
    for (unsigned i = 0; i < req->size; i++) out[i] = (unsigned char)(0x40 + i);
    return 0;
}

int main(void)
{
    char path[] = "/tmp/oa-spi-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    unsigned char content[16];
    for (unsigned i = 0; i < sizeof content; i++) content[i] = (unsigned char)i;
    assert(write(fd, content, sizeof content) == (ssize_t)sizeof content);
    close(fd);

    oa_spi spi;
    assert(oa_spi_open(&spi, path, FAKE_DEVICE) == 0);
    assert(spi.file && spi.fd == -1 && spi.size == sizeof content);
    unsigned char got[16];
    memset(got, 0, sizeof got);
    assert(oa_spi_read(&spi, 4, 4, got) == 1);
    assert(got[0] == 4 && got[1] == 5 && got[2] == 6 && got[3] == 7);
    assert(oa_spi_read(&spi, 0, 16, got) == 1 && got[15] == 15);
    assert(oa_spi_read(&spi, 14, 4, got) == 0);   /* out of range */
    assert(oa_spi_read(&spi, 16, 1, got) == 0);
    assert(oa_spi_read(&spi, 0, 0, got) == 0);    /* zero length */
    assert(oa_spi_read(&spi, 0, 1, NULL) == 0);   /* NULL buffer */
    oa_spi_close(&spi);
    assert(spi.file == NULL && spi.fd == -1 && spi.size == 0);
    unlink(path);

    /* File absent -> recovered device backend and ioctl request. */
    assert(oa_spi_open(&spi, path, FAKE_DEVICE) == 0);
    assert(spi.file == NULL && spi.fd == 123 && spi.size == 0);
    memset(got, 0, sizeof got);
    assert(oa_spi_read(&spi, 0x20, 8, got) == 1);
    assert(ioctl_calls == 1);
    assert(got[0] == 0x40 && got[7] == 0x47);
    fail_ioctl = 1;
    assert(oa_spi_read(&spi, 0x20, 8, got) == 0);
    oa_spi_close(&spi);
    assert(fake_closes == 1);
    oa_spi_close(&spi); /* idempotent */
    assert(fake_closes == 1);

    oa_spi empty;
    assert(oa_spi_open(&empty, "", "") == -ENOENT);
    puts("ok spi dev: recovered file/device backends, 24-byte read request, bounds/failure paths");
    return 0;
}
