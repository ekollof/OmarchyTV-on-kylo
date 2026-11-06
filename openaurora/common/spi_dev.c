#include "openaurora/spi_dev.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct spi_read_req {
    uint32_t offset;
    uint32_t size;
    uintptr_t buf; /* 32-bit host pointer on the TV, 24-byte ARM request */
    uint32_t reserved[3];
};

int oa_spi_open(oa_spi *spi, const char *file_path, const char *device_path)
{
    if (!spi) return -EINVAL;
    spi->fd = -1;
    spi->file = NULL;
    spi->size = 0;

    if (file_path && *file_path) {
        FILE *f = fopen(file_path, "rb+");
        if (!f) f = fopen(file_path, "rb");
        if (f) {
            if (fseek(f, 0, SEEK_END) == 0) {
                long end = ftell(f);
                if (end >= 0 && fseek(f, 0, SEEK_SET) == 0) {
                    spi->file = f;
                    spi->size = (uint32_t)end;
                    return 0;
                }
            }
            fclose(f);
            return -EIO;
        }
    }

    if (device_path && *device_path) {
        int fd = open(device_path, O_RDWR | O_CLOEXEC);
        if (fd < 0) return -errno;
        spi->fd = fd;
        return 0;
    }
    return -ENOENT;
}

int oa_spi_read(oa_spi *spi, uint32_t offset, uint32_t len, void *buf)
{
    if (!spi || !buf || len == 0) return 0;
    if (spi->file) {
        if (offset > spi->size || len > spi->size - offset) return 0;
        if (fseek((FILE *)spi->file, (long)offset, SEEK_SET) != 0) return 0;
        return fread(buf, 1, len, (FILE *)spi->file) == len;
    }
    if (spi->fd >= 0) {
        struct spi_read_req req;
        memset(&req, 0, sizeof req);
        req.offset = offset;
        req.size = len;
        req.buf = (uintptr_t)buf;
        return ioctl(spi->fd, OA_SPI_READ_IOCTL, &req) == 0;
    }
    return 0;
}

void oa_spi_close(oa_spi *spi)
{
    if (!spi) return;
    if (spi->file) {
        fclose((FILE *)spi->file);
        spi->file = NULL;
    }
    if (spi->fd >= 0) {
        close(spi->fd);
        spi->fd = -1;
    }
    spi->size = 0;
}
