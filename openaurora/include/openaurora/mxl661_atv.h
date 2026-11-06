/* MxL661 analog (ATV) RF/IF tables and sequence.
 *
 * The vendor's NTSC/PAL/SECAM property, RF-LUT and frequency-LUT tables are
 * part of the TV's own aurora.elf. They are NOT embedded in this source or in
 * any build product: the loader reads the 0xab8-byte table region from the
 * frontend's ELF at runtime, verifies its CRC32, and the sequence code uses
 * that copy. Host tests either provide a synthetic table set or point the
 * loader at the private dump.
 */
#ifndef OPENAURORA_MXL661_ATV_H
#define OPENAURORA_MXL661_ATV_H

#include <stdint.h>

#include "openaurora/mxl661.h"

#define OA_MXL661_ATV_REGION_ADDR  0x017681c8u
#define OA_MXL661_ATV_REGION_LEN   0xab8u
#define OA_MXL661_ATV_REGION_CRC32 0x93a05fffu

struct oa_mxl661_atv_tables {
    const uint8_t *ntsc_props;
    const uint8_t *pal_props;
    const uint8_t *secam_props;
    const uint8_t *ntsc_16m_regs;
    const uint8_t *ntsc_24m_regs;
    const uint8_t *ntsc_hrc_regs;
    const uint8_t *ntsc_hif_regs;
    const uint8_t *pal_bg_7_hif_regs;
    const uint8_t *pal_bg_8_hif_regs;
    const uint8_t *pal_d_hif_regs;
    const uint8_t *pal_i_hif_regs;
    const uint8_t *pal_lif_regs;
    const uint32_t *ntsc_lif_lut;
    const uint32_t *ntsc_hif_lut;
    const uint32_t *pal_bg_7_16hif_lut;
    const uint32_t *pal_bg_7_16lif_lut;
    const uint32_t *pal_bg_8_16hif_lut;
    const uint32_t *pal_bg_8_16lif_lut;
    const uint32_t *pal_bg_7_24_lut;
    const uint32_t *pal_bg_8_24_lut;
    const uint32_t *pal_i_24_lut;
    const uint32_t *pal_i_16hif_lut;
    const uint32_t *pal_i_16lif_lut;
    const uint32_t *pal_d_24_lut;
    const uint32_t *pal_d_16hif_lut;
    const uint32_t *pal_d_16lif_lut;
    const uint32_t *secam_l_24_lut;
    const uint32_t *secam_l_16_lut;
    uint32_t region[OA_MXL661_ATV_REGION_LEN / 4];
};

/* Read and validate the vendor table region from a 32-bit little-endian ARM
 * ELF (the TV's /vendor/com.nvt.idtv/aurora.elf). 0 = success. */
int oa_mxl661_atv_load(const char *elf_path, struct oa_mxl661_atv_tables *out);

/* ConfigTuner case-9 analog standard sub-switch (param[2]). */
enum {
    OA_MXL661_ATV_NTSC = 0,
    OA_MXL661_ATV_PAL = 1,
    OA_MXL661_ATV_PAL_I = 2,
    OA_MXL661_ATV_PAL_D = 3,
    OA_MXL661_ATV_SECAM = 4,
    OA_MXL661_ATV_SECAM_L = 5
};

struct oa_mxl661_analog_params {
    uint32_t freq_hz;
    uint32_t if_khz;    /* below 0x89b2 = LIF, at/above = HIF */
    uint32_t bandwidth; /* 0 = the vendor SetRF analog default 2 */
    int standard;       /* OA_MXL661_ATV_* */
    int start;          /* case 0xd param[2] */
    int xtal_24mhz;     /* gTunerConfig XtalFreqSel != 0 */
};

/* Analog IF (case 9), start tune (0xd) and the shared channel tune (0xb).
 * `tables` must come from oa_mxl661_atv_load() or a test fixture with the same
 * table layout. The per-address case-9 calibration (gTunerConfig/ConfigDevice
 * records) is not modeled; the analog 0x6d=0x80 common-tail write assumes the
 * initialized device record for this address. */
int oa_mxl661_tune_analog(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                          const struct oa_mxl661_analog_params *params,
                          const struct oa_mxl661_atv_tables *tables);

/* SAL_TUNER_MxL661_GetATVStatus lock decode. Path 1/2 uses 0x2b:
 * ((v & 3) >> 1) & (v & 1); path 3 uses 0x2c: ((v & 0xf) >> 3) & 1.
 * Returns 1 for a recognized path (and sets *locked), 0 otherwise. */
int oa_mxl661_atv_status_lock(int path, uint8_t value, uint8_t *locked);

#endif
