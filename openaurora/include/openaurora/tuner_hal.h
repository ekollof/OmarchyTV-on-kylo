/* HAL tuner bus resolution and register read/write ABI.
 *
 * Recovered from aurora.elf:
 *   HAL_TUNER_Init        @0x0084c164
 *   HAL_TUNER_ReadBase    @0x0084c364
 *   HAL_TUNER_WriteBase   @0x0084c304
 *   HAL_TUNER_Read/Write  @0x0084c408/0x0084c3c4 (bus wrapper)
 *
 * HAL_TUNER_Init only resolves four IIC bus IDs; each is read from byte +0x19
 * of DEV_IIC_GetDescription(0..3):
 *   0 tuner      -> global 0x0175eb78
 *   1 sat        -> 0x0175eb76
 *   2 ext demod  -> 0x0175eb77
 *   3 LNB        -> 0x0175eb75
 * A missing description falls back to bus 0. The globals start at 0xff and are
 * only resolved while they still hold 0xff.
 *
 * HAL_TUNER_WriteBase(bus, reg, buf, len) builds a context whose first two
 * words are {bus, reg}, followed by two zero words, a 400-unit timeout and a
 * zero word, then calls Hal_Iic_Write(ctx, &zero_byte, len, buf). Success is
 * Hal's rc == 0 (the ARM return is clz(rc) >> 5).
 */
#ifndef OPENAURORA_TUNER_HAL_H
#define OPENAURORA_TUNER_HAL_H
#include <stdint.h>

#define OA_TUNER_BUS_UNRESOLVED 0xffu
#define OA_TUNER_IIC_TIMEOUT 400u

#define OA_TUNER_BUSES_INIT { OA_TUNER_BUS_UNRESOLVED, OA_TUNER_BUS_UNRESOLVED, \
                              OA_TUNER_BUS_UNRESOLVED, OA_TUNER_BUS_UNRESOLVED }

typedef struct {
    uint8_t tuner;
    uint8_t sat;
    uint8_t ext_demod;
    uint8_t lnb;
} oa_tuner_buses;

/* Same shape as Hal_Iic_Read/Write: 0 = success. */
typedef struct {
    int (*iic_read)(const void *ctx, const void *value, unsigned len,
                    void *buf, void *ud);
    int (*iic_write)(const void *ctx, const void *value, unsigned len,
                     void *buf, void *ud);
} oa_tuner_iic_ops;

/* DEV_IIC_GetDescription callback: returns the description pointer or NULL. */
typedef const uint8_t *(*oa_tuner_desc_fn)(int index, void *ud);

/* Resolve every bus still equal to 0xff. 0 = ok (missing descriptions become
 * bus 0, like the vendor's explicit default branch). */
int oa_tuner_bus_ids(oa_tuner_buses *buses, oa_tuner_desc_fn get, void *ud);

/* Vendor bool convention: 1 = Hal rc 0, 0 = failure. */
int oa_tuner_read_base(const oa_tuner_iic_ops *ops, void *ud, uint8_t bus,
                       uint8_t reg, uint8_t *buf, uint8_t len);
int oa_tuner_write_base(const oa_tuner_iic_ops *ops, void *ud, uint8_t bus,
                        uint8_t reg, const uint8_t *buf, uint8_t len);

#endif
