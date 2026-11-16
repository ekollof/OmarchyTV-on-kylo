/* ARM qemu probe: drive the real libNDAL3 Hal_Iic_* and the project's
 * recovered SPI transport against the opt-in stateful TV I/O model.
 * No TV, kernel driver or real bus access. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/spi_dev.h"

/* libNDAL3 exports these with void returns; the transfer result is ignored by
 * the vendor wrappers. */
extern void Hal_Iic_Init(void);
extern void Hal_Iic_Read(const void *ctx, const void *value, unsigned len,
                         void *buf);
extern void Hal_Iic_Write(const void *ctx, const void *value, unsigned len,
                          void *buf);

int main(void)
{
    /* 24-byte recovered context: {bus, addr<<1, subaddr_len, 0, timeout, 0}. */
    uint32_t ctx[6] = { 0u, 0x34u << 1, 1u, 0u, 100u, 0u };
    uint8_t sub = 0x03u;
    uint8_t data[4] = { 0 };

    Hal_Iic_Init();
    Hal_Iic_Write(ctx, &sub, 1, data);
    Hal_Iic_Read(ctx, &sub, sizeof data, data);
    printf("iic addr=0x34 len=%u byte0=%02x byte1=%02x\n",
           (unsigned)sizeof data, data[0], data[1]);

    oa_spi spi;
    memset(&spi, 0, sizeof spi);
    int open_rc = oa_spi_open(&spi, "", "/dev/ntkspi");
    uint8_t sbuf[8] = { 0 };
    int read_rc = oa_spi_read(&spi, 0x10u, 4u, sbuf);
    printf("spi open=%d read=%d byte0=%02x byte1=%02x\n",
           open_rc, read_rc, sbuf[0], sbuf[1]);
    oa_spi_close(&spi);
    return 0;
}
