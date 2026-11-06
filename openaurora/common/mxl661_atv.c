/* MxL661 analog RF/IF path. The vendor tables are read from the TV's own
 * aurora.elf at runtime (see mxl661_atv.h); no vendor data is compiled in. */
#include "openaurora/mxl661_atv.h"

#include <elf.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/mxl661_rf.h"

/* Offsets of the vendor tables inside the CRC-checked region. */
enum {
    OFF_NTSC_PROPS = 0x000,
    OFF_PAL_PROPS = 0x054,
    OFF_SECAM_PROPS = 0x0ac,
    OFF_NTSC_16M_REGS = 0x250,
    OFF_NTSC_24M_REGS = 0x270,
    OFF_NTSC_HRC_REGS = 0x290,
    OFF_NTSC_HIF_REGS = 0x2b4,
    OFF_PAL_BG_7_HIF_REGS = 0x2d4,
    OFF_PAL_BG_8_HIF_REGS = 0x2f0,
    OFF_PAL_D_HIF_REGS = 0x30c,
    OFF_PAL_I_HIF_REGS = 0x328,
    OFF_PAL_LIF_REGS = 0x344,
    OFF_NTSC_LIF_LUT = 0x480,
    OFF_NTSC_HIF_LUT = 0x510,
    OFF_PAL_BG_7_16HIF_LUT = 0x620,
    OFF_PAL_BG_7_16LIF_LUT = 0x660,
    OFF_PAL_BG_8_16HIF_LUT = 0x6b8,
    OFF_PAL_BG_8_16LIF_LUT = 0x700,
    OFF_PAL_BG_7_24_LUT = 0x698,
    OFF_PAL_BG_8_24_LUT = 0x780,
    OFF_PAL_I_24_LUT = 0xa48,
    OFF_PAL_I_16HIF_LUT = 0x958,
    OFF_PAL_I_16LIF_LUT = 0xa00,
    OFF_PAL_D_24_LUT = 0x918,
    OFF_PAL_D_16HIF_LUT = 0x7c0,
    OFF_PAL_D_16LIF_LUT = 0x838,
    OFF_SECAM_L_24_LUT = 0xa98,
    OFF_SECAM_L_16_LUT = 0xa78
};

static uint32_t crc32_region(const uint8_t *p, size_t n)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
    return crc ^ 0xffffffffu;
}

static int read_segment(FILE *f, const Elf32_Phdr *ph, int n, uint32_t vaddr,
                        void *dst, size_t len)
{
    for (int i = 0; i < n; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;
        uint32_t start = ph[i].p_vaddr;
        uint32_t end = start + ph[i].p_filesz;
        if (vaddr < start || vaddr + len > end)
            continue;
        long off = (long)ph[i].p_offset + (long)(vaddr - start);
        if (fseek(f, off, SEEK_SET) != 0)
            return -1;
        return fread(dst, 1, len, f) == len ? 0 : -1;
    }
    return -1;
}

int oa_mxl661_atv_load(const char *elf_path, struct oa_mxl661_atv_tables *out)
{
    if (!elf_path || !out)
        return -1;
    FILE *f = fopen(elf_path, "rb");
    if (!f)
        return -1;
    int rc = -1;
    Elf32_Ehdr eh;
    Elf32_Phdr ph[32];
    uint8_t region[OA_MXL661_ATV_REGION_LEN];

    if (fread(&eh, sizeof eh, 1, f) != 1)
        goto done;
    if (memcmp(eh.e_ident, ELFMAG, SELFMAG) ||
        eh.e_ident[EI_CLASS] != ELFCLASS32 ||
        eh.e_ident[EI_DATA] != ELFDATA2LSB ||
        eh.e_machine != EM_ARM ||
        eh.e_phnum == 0 || eh.e_phnum > 32 ||
        eh.e_phentsize != sizeof(Elf32_Phdr))
        goto done;
    if (fseek(f, (long)eh.e_phoff, SEEK_SET) != 0 ||
        fread(ph, sizeof ph[0], eh.e_phnum, f) != eh.e_phnum)
        goto done;
    if (read_segment(f, ph, eh.e_phnum, OA_MXL661_ATV_REGION_ADDR,
                     region, sizeof region) != 0)
        goto done;
    if (crc32_region(region, sizeof region) != OA_MXL661_ATV_REGION_CRC32)
        goto done;

    memcpy(out->region, region, sizeof region);
    const uint8_t *b = (const uint8_t *)out->region;
    out->ntsc_props = b + OFF_NTSC_PROPS;
    out->pal_props = b + OFF_PAL_PROPS;
    out->secam_props = b + OFF_SECAM_PROPS;
    out->ntsc_16m_regs = b + OFF_NTSC_16M_REGS;
    out->ntsc_24m_regs = b + OFF_NTSC_24M_REGS;
    out->ntsc_hrc_regs = b + OFF_NTSC_HRC_REGS;
    out->ntsc_hif_regs = b + OFF_NTSC_HIF_REGS;
    out->pal_bg_7_hif_regs = b + OFF_PAL_BG_7_HIF_REGS;
    out->pal_bg_8_hif_regs = b + OFF_PAL_BG_8_HIF_REGS;
    out->pal_d_hif_regs = b + OFF_PAL_D_HIF_REGS;
    out->pal_i_hif_regs = b + OFF_PAL_I_HIF_REGS;
    out->pal_lif_regs = b + OFF_PAL_LIF_REGS;
    out->ntsc_lif_lut = (const uint32_t *)(b + OFF_NTSC_LIF_LUT);
    out->ntsc_hif_lut = (const uint32_t *)(b + OFF_NTSC_HIF_LUT);
    out->pal_bg_7_16hif_lut = (const uint32_t *)(b + OFF_PAL_BG_7_16HIF_LUT);
    out->pal_bg_7_16lif_lut = (const uint32_t *)(b + OFF_PAL_BG_7_16LIF_LUT);
    out->pal_bg_8_16hif_lut = (const uint32_t *)(b + OFF_PAL_BG_8_16HIF_LUT);
    out->pal_bg_8_16lif_lut = (const uint32_t *)(b + OFF_PAL_BG_8_16LIF_LUT);
    out->pal_bg_7_24_lut = (const uint32_t *)(b + OFF_PAL_BG_7_24_LUT);
    out->pal_bg_8_24_lut = (const uint32_t *)(b + OFF_PAL_BG_8_24_LUT);
    out->pal_i_24_lut = (const uint32_t *)(b + OFF_PAL_I_24_LUT);
    out->pal_i_16hif_lut = (const uint32_t *)(b + OFF_PAL_I_16HIF_LUT);
    out->pal_i_16lif_lut = (const uint32_t *)(b + OFF_PAL_I_16LIF_LUT);
    out->pal_d_24_lut = (const uint32_t *)(b + OFF_PAL_D_24_LUT);
    out->pal_d_16hif_lut = (const uint32_t *)(b + OFF_PAL_D_16HIF_LUT);
    out->pal_d_16lif_lut = (const uint32_t *)(b + OFF_PAL_D_16LIF_LUT);
    out->secam_l_24_lut = (const uint32_t *)(b + OFF_SECAM_L_24_LUT);
    out->secam_l_16_lut = (const uint32_t *)(b + OFF_SECAM_L_16_LUT);
    rc = 0;
done:
    fclose(f);
    return rc;
}

static int wr(const oa_mxl661_ops *ops, void *ud, uint8_t addr, uint8_t reg,
              uint8_t value)
{
    return ops->write_reg(ud, addr, reg, value) != 0;
}

static int rd(const oa_mxl661_ops *ops, void *ud, uint8_t addr, uint8_t reg,
              uint8_t *value)
{
    *value = 0;
    return ops->read_reg(ud, addr, reg, value) != 0;
}

static void msleep(const oa_mxl661_ops *ops, void *ud, unsigned ms)
{
    if (ops->sleep_ms)
        ops->sleep_ms(ud, ms);
}

/* Ctrl_SetRfFreqLutReg: key 1 is the default spur image; otherwise the first
 * key within +/-500 kHz selects it. The image bytes go to the four MxL601
 * spur registers. */
static int set_rf_freq_lut(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                           uint32_t freq_hz, const uint32_t *lut)
{
    static const uint8_t spur_reg[4] = { 0x49, 0xdb, 0xea, 0xeb };
    uint32_t image = 0;
    for (size_t i = 0; lut && lut[i]; i += 2) {
        if (lut[i] == 1) {
            image = lut[i + 1];
            continue;
        }
        if (freq_hz >= lut[i] - 500000u && freq_hz <= lut[i] + 500000u) {
            image = lut[i + 1];
            break;
        }
    }
    int fail = 0;
    for (int i = 0; i < 4; i++)
        fail |= wr(ops, ud, addr, spur_reg[i],
                   (uint8_t)(image >> (8 * i)));
    return fail;
}

static int analog_1a_69(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                        uint8_t value_69)
{
    uint8_t v = 0;
    int fail = rd(ops, ud, addr, 0x1a, &v);
    int act = (v & 0xff) == 4;
    if (!act && (v & 0xff) == 2) {
        fail |= rd(ops, ud, addr, 0x5f, &v);
        act = (v & 0xff) != 0;
    }
    if (act) {
        fail |= wr(ops, ud, addr, 0x00, 0x01);
        fail |= wr(ops, ud, addr, 0x69, value_69);
        fail |= wr(ops, ud, addr, 0x00, 0x00);
    }
    return fail;
}

static int set_if_analog(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                         const struct oa_mxl661_analog_params *p,
                         const struct oa_mxl661_atv_tables *t)
{
    const uint8_t *props = NULL;
    const uint8_t *regs = NULL;
    int lif = p->if_khz < 0x89b2u;
    int fail = 0;
    switch (p->standard) {
    case OA_MXL661_ATV_NTSC:
        props = t->ntsc_props;
        if (lif)
            regs = p->xtal_24mhz ? t->ntsc_24m_regs : t->ntsc_16m_regs;
        else
            regs = t->ntsc_hif_regs;
        break;
    case OA_MXL661_ATV_PAL:
        props = t->pal_props;
        if (lif)
            regs = t->pal_lif_regs;
        break;
    case OA_MXL661_ATV_PAL_I:
        fail |= wr(ops, ud, addr, 0x0c, 0x0b);
        props = t->pal_props;
        regs = lif ? t->pal_lif_regs : t->pal_i_hif_regs;
        break;
    case OA_MXL661_ATV_PAL_D:
        fail |= wr(ops, ud, addr, 0x0c, 0x00);
        props = t->pal_props;
        regs = lif ? t->pal_lif_regs : t->pal_d_hif_regs;
        break;
    case OA_MXL661_ATV_SECAM:
        fail |= wr(ops, ud, addr, 0x0c, 0x0b);
        props = t->secam_props;
        break;
    case OA_MXL661_ATV_SECAM_L:
        fail |= wr(ops, ud, addr, 0x0c, 0x0c);
        props = t->secam_props;
        break;
    default:
        return -1;
    }
    fail |= oa_mxl661_program_registers(ops, ud, addr, props);
    fail |= wr(ops, ud, addr, 0x5a, lif ? 0x7e : 0xd9);
    fail |= wr(ops, ud, addr, 0x5b, lif ? 0x10 : 0x16);
    fail |= wr(ops, ud, addr, 0x8a,
               (p->standard == OA_MXL661_ATV_PAL_I && lif) ? 0x28 : 0x00);
    if (p->standard == OA_MXL661_ATV_NTSC && lif)
        fail |= wr(ops, ud, addr, 0xdd, p->xtal_24mhz ? 0x01 : 0x00);
    if (regs)
        fail |= oa_mxl661_program_registers(ops, ud, addr, regs);
    if (fail)
        return -1;
    fail |= wr(ops, ud, addr, 0x6d, 0x0a);
    fail |= wr(ops, ud, addr, 0x03, 0x00);
    fail |= wr(ops, ud, addr, 0x03, 0x01);
    msleep(ops, ud, 50);
    fail |= analog_1a_69(ops, ud, addr, 0x01);
    return fail ? -1 : 0;
}

static int start_tune_analog(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                             const struct oa_mxl661_analog_params *p,
                             const struct oa_mxl661_atv_tables *t,
                             uint32_t bandwidth)
{
    int lif = p->if_khz < 0x89b2u;
    uint8_t v = 0;
    int fail = 0;

    fail |= rd(ops, ud, addr, 0x1a, &v);
    if ((v & 0xff) == 4) {
        fail |= rd(ops, ud, addr, 0x58, &v);
        fail |= wr(ops, ud, addr, 0x58, (uint8_t)(v & 0xfb));
        fail |= rd(ops, ud, addr, 0x57, &v);
        fail |= wr(ops, ud, addr, 0x57, (uint8_t)(v & 0xf7));
    }
    if (!p->start) {
        fail |= wr(ops, ud, addr, 0x7c,
                   p->freq_hz < 700000000u ? 0x1f : 0x9f);
        fail |= wr(ops, ud, addr, 0x00, 0x01);
        fail |= wr(ops, ud, addr, 0x31,
                   p->freq_hz < 700000000u ? 0x81 : 0x91);
        fail |= wr(ops, ud, addr, 0x00, 0x00);
    }

    const uint32_t *lut = NULL;
    switch (p->standard) {
    case OA_MXL661_ATV_NTSC:
        fail |= oa_mxl661_program_registers(ops, ud, addr, t->ntsc_hrc_regs);
        lut = lif ? t->ntsc_lif_lut : t->ntsc_hif_lut;
        break;
    case OA_MXL661_ATV_PAL:
        /* SetRF always passes bandwidth 2 for analog; 1/2 map 0xc to 9/10. */
        fail |= wr(ops, ud, addr, 0x0c, bandwidth == 1 ? 0x09 : 0x0a);
        fail |= analog_1a_69(ops, ud, addr, 0x00);
        if (!p->xtal_24mhz) {
            fail |= oa_mxl661_program_registers(ops, ud, addr,
                                                t->pal_bg_8_hif_regs);
            lut = lif ? t->pal_bg_8_16lif_lut : t->pal_bg_8_16hif_lut;
        } else {
            fail |= oa_mxl661_program_registers(ops, ud, addr, t->pal_lif_regs);
            lut = t->pal_bg_8_24_lut;
        }
        break;
    case OA_MXL661_ATV_PAL_I:
        lut = p->xtal_24mhz ? t->pal_i_24_lut
                            : (lif ? t->pal_i_16lif_lut
                                   : t->pal_i_16hif_lut);
        break;
    case OA_MXL661_ATV_PAL_D:
        lut = p->xtal_24mhz ? t->pal_d_24_lut
                            : (lif ? t->pal_d_16lif_lut
                                   : t->pal_d_16hif_lut);
        if (p->freq_hz < 358000000u) {
            fail |= wr(ops, ud, addr, 0x93, 0x44);
            fail |= wr(ops, ud, addr, 0x94, 0x55);
            fail |= wr(ops, ud, addr, 0xba, 0x50);
            fail |= wr(ops, ud, addr, 0x5d, 0x0f);
        } else {
            fail |= wr(ops, ud, addr, 0x93, 0x33);
            fail |= wr(ops, ud, addr, 0x94, 0x44);
            fail |= wr(ops, ud, addr, 0xba, 0x40);
            fail |= wr(ops, ud, addr, 0x5d, 0x0e);
        }
        break;
    case OA_MXL661_ATV_SECAM:
        break;
    case OA_MXL661_ATV_SECAM_L:
        lut = p->xtal_24mhz ? t->secam_l_24_lut : t->secam_l_16_lut;
        break;
    default:
        return -1;
    }
    if (lut)
        fail |= set_rf_freq_lut(ops, ud, addr, p->freq_hz, lut);

    fail |= rd(ops, ud, addr, 0x0d, &v);
    fail |= wr(ops, ud, addr, 0x0d,
               (uint8_t)((v & 0xfe) | (p->start ? 1 : 0)));
    uint8_t f = (bandwidth - 0x23u < 2u) ? 0x20 : (uint8_t)bandwidth;
    fail |= wr(ops, ud, addr, 0x0f, f);
    uint32_t tdiv = (uint32_t)(((uint64_t)p->freq_hz * 0x10624dd3u) >> 32);
    tdiv &= 0xffffffc0u;
    tdiv = (uint32_t)(((uint64_t)(tdiv + 500u) * 0x10624dd3u) >> 32);
    fail |= wr(ops, ud, addr, 0x10, (uint8_t)((tdiv & 0x3fff) >> 6));
    fail |= wr(ops, ud, addr, 0x11, (uint8_t)((tdiv & 0x3fffff) >> 14));
    /* The initialized per-address device record makes the analog 0x6d write
     * the constant 0x80 (both record bytes +1/+2 are zero). */
    fail |= wr(ops, ud, addr, 0x6d, 0x80);
    msleep(ops, ud, 30);
    fail |= oa_mxl661_channel_tune(ops, ud, addr);
    return fail ? -1 : 0;
}

int oa_mxl661_tune_analog(const oa_mxl661_ops *ops, void *ud, uint8_t addr,
                          const struct oa_mxl661_analog_params *params,
                          const struct oa_mxl661_atv_tables *tables)
{
    if (!ops || !ops->write_reg || !ops->read_reg || !params || !tables)
        return -1;
    if (params->standard < OA_MXL661_ATV_NTSC ||
        params->standard > OA_MXL661_ATV_SECAM_L)
        return -1;
    uint32_t bandwidth = params->bandwidth ? params->bandwidth : 2;
    int fail = set_if_analog(ops, ud, addr, params, tables);
    fail |= start_tune_analog(ops, ud, addr, params, tables, bandwidth);
    return fail ? -1 : 0;
}
