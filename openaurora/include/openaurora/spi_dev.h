/* SPI device transport recovered from aurora DEV_SPI_*.
 *
 * Vendor order: DEV_SPI_Init creates the lock/debug registrations, then
 * DEV_SPI_Open(0) prefers /conf/spi.bin ("rb+") and otherwise opens
 * /dev/ntkspi O_RDWR. Reads use fseek/fread on the file backend or
 * ioctl(0x8018531e) with a 24-byte {offset,size,buf,0,0,0} request on the
 * device backend. DEV_SPI_Read returns 1 on a full read, 0 on failure.
 *
 * This is transport groundwork for a future NIM/CI consumer, not a claim of
 * SPI hardware ownership. No module currently loads it. */
#ifndef OPENAURORA_SPI_DEV_H
#define OPENAURORA_SPI_DEV_H
#include <stddef.h>
#include <stdint.h>

#define OA_SPI_DEFAULT_FILE "/conf/spi.bin"
#define OA_SPI_DEFAULT_DEVICE "/dev/ntkspi"
#define OA_SPI_READ_IOCTL 0x8018531eu

typedef struct {
    int fd;        /* device fd, -1 when the file backend is active */
    void *file;    /* FILE* for the file backend, NULL for the device */
    uint32_t size; /* file size, 0 for the device backend */
} oa_spi;

/* Tries the file backend first (vendor order), then the device. Returns 0 on
 * success, -errno on failure. Empty/NULL paths skip that backend. */
int oa_spi_open(oa_spi *spi, const char *file_path, const char *device_path);

/* 1 = full read, 0 = failure (vendor convention). Rejects NULL, zero length
 * and out-of-range file reads. */
int oa_spi_read(oa_spi *spi, uint32_t offset, uint32_t len, void *buf);

void oa_spi_close(oa_spi *spi);

#endif
