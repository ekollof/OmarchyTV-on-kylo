#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/efsl_profile.h"
#include "openaurora/tuner_identity.h"

#define HDR 0x30u
#define ENTRIES 0x44u
#define VALS 0xE4u
#define PROF_SIZE 0x200u
#define ROOT_COUNT 8u

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void entry(uint8_t *buf, unsigned index, const char *key, uint8_t type,
                  uint32_t size, uint32_t value)
{
    uint8_t *e = buf + ENTRIES + index * OA_EFSL_ENTRY_SIZE;
    put32(e, oa_efsl_hash(key));
    put32(e + 4, type);
    put32(e + 8, size);
    put32(e + 12, value);
    put32(e + 16, 0);
}

/* Synthetic container with the same shape as the vendor profiles:
 * root -> TunerTypes { Si2151, Si2157, Empty, TunerType } and
 *         SwProfileVersion { Version }, plus a root type-4 value. */
static void build_profile(uint8_t *buf, size_t size, const char *tuner)
{
    assert(size >= PROF_SIZE);
    assert(strlen(tuner) == 6);
    memset(buf, 0, size);
    memcpy(buf, "EFSL", 4);
    put32(buf + 0x20, 0);
    put32(buf + 0x38, ROOT_COUNT);

    entry(buf, 0, "TunerTypes", 1, 4, 0);
    entry(buf, 1, "Si2151", 1, 0, 0);
    entry(buf, 2, "Si2157", 1, 0, 0);
    entry(buf, 3, "Empty", 1, 0, 0);
    entry(buf, 4, "TunerType", 3, 7, VALS);
    entry(buf, 5, "SwProfileVersion", 1, 1, 0);
    entry(buf, 6, "Version", 2, 3, VALS + 7);
    entry(buf, 7, "RootValue", 4, 4, VALS + 10);

    memcpy(buf + VALS, tuner, 7); /* six chars + NUL, like the real value */
    memcpy(buf + VALS + 7, "39", 3);
    buf[VALS + 10] = 0x11;
    buf[VALS + 11] = 0x22;
    buf[VALS + 12] = 0x33;
    buf[VALS + 13] = 0x44;
}

static void test_hash(void)
{
    /* Values located in the shipped mb230 profiles with this hash. */
    assert(oa_efsl_hash("SwProfileVersion") == 0x9af10992u);
    assert(oa_efsl_hash("TunerType") == 0x04461a6au);
    assert(oa_efsl_hash("TunerTypes") == 0xc4791570u);
    assert(oa_efsl_hash("Version") == 0x74a04762u);
    assert(oa_efsl_hash("Si2151") == 0x3e73b5e9u);
    assert(oa_efsl_hash("Si2157") == 0x3e3035e9u);
    assert(oa_efsl_hash("Si2158") == 0x079a6f30u);
    assert(oa_efsl_hash(NULL) == 0);
}

static void test_parser(void)
{
    static uint8_t buf[PROF_SIZE];
    build_profile(buf, sizeof buf, "Si2157");

    oa_efsl_profile p;
    oa_efsl_section root, sec, sub;
    char val[32];
    size_t len = 0;
    uint8_t raw[4] = { 0 };

    assert(oa_efsl_open(&p, buf, sizeof buf) == 0);
    assert(oa_efsl_root(&p, &root) == 0 && root.count == ROOT_COUNT);

    assert(oa_efsl_open_section(&p, &root, "TunerTypes", &sec) == 0 &&
           sec.count == 4);
    assert(oa_efsl_read_value(&p, &sec, "TunerType", val, sizeof val, &len) ==
           0);
    assert(len == 7 && memcmp(val, "Si2157\0", 7) == 0);

    /* A subsection with the same key name is not a value. */
    assert(oa_efsl_read_value(&p, &sec, "Si2151", val, sizeof val, &len) == -1);
    assert(oa_efsl_read_value(&p, &sec, "Missing", val, sizeof val, &len) ==
           -1);
    /* A value key is not a section. */
    assert(oa_efsl_open_section(&p, &root, "RootValue", &sec) == -1);

    assert(oa_efsl_open_section(&p, &root, "SwProfileVersion", &sub) == 0 &&
           sub.count == 1);
    assert(oa_efsl_read_value(&p, &sub, "Version", val, sizeof val, &len) == 0);
    assert(len == 3 && memcmp(val, "39\0", 3) == 0);

    assert(oa_efsl_read_value(&p, &root, "RootValue", raw, sizeof raw, &len) ==
           0);
    assert(len == 4 && raw[0] == 0x11 && raw[1] == 0x22 && raw[2] == 0x33 &&
           raw[3] == 0x44);
    /* readValue rejects a value larger than the caller's buffer. */
    assert(oa_efsl_read_value(&p, &root, "RootValue", raw, 3, &len) == -1);
}

static void test_malformed(void)
{
    static uint8_t buf[PROF_SIZE];
    oa_efsl_profile p;
    oa_efsl_section root, sec;
    char val[32];
    size_t len = 0;

    build_profile(buf, sizeof buf, "Si2157");

    /* Wrong magic. */
    buf[0] = 'X';
    assert(oa_efsl_open(&p, buf, sizeof buf) == -1);
    buf[0] = 'E';

    /* ROMProfile::initialize rejects a nonzero base word. */
    put32(buf + 0x20, 1);
    assert(oa_efsl_open(&p, buf, sizeof buf) == -1);
    put32(buf + 0x20, 0);

    /* Truncated header. */
    assert(oa_efsl_open(&p, buf, HDR + 4) == -1);

    /* Root count larger than the buffer. */
    put32(buf + 0x38, 0xffffffffu);
    assert(oa_efsl_open(&p, buf, sizeof buf) == -1);
    put32(buf + 0x38, ROOT_COUNT);

    assert(oa_efsl_open(&p, buf, sizeof buf) == 0);
    assert(oa_efsl_root(&p, &root) == 0);

    /* Value offset outside the file. */
    put32(buf + ENTRIES + 4 * OA_EFSL_ENTRY_SIZE + 12, 0x1ff0u);
    assert(oa_efsl_open_section(&p, &root, "TunerTypes", &sec) == 0);
    assert(oa_efsl_read_value(&p, &sec, "TunerType", val, sizeof val, &len) ==
           -1);
    put32(buf + ENTRIES + 4 * OA_EFSL_ENTRY_SIZE + 12, VALS);

    /* Child count that runs past the container. */
    put32(buf + ENTRIES + 8, 0xffu);
    assert(oa_efsl_open_section(&p, &root, "TunerTypes", &sec) == -1);
    put32(buf + ENTRIES + 8, 4);

    /* Value length larger than the remaining file. */
    put32(buf + ENTRIES + 7 * OA_EFSL_ENTRY_SIZE + 8, 0xffffffffu);
    assert(oa_efsl_read_value(&p, &root, "RootValue", val, sizeof val, &len) ==
           -1);
}

static void test_identity(void)
{
    static uint8_t si2151[PROF_SIZE];
    static uint8_t si2157[PROF_SIZE];
    oa_efsl_profile p;

    build_profile(si2151, sizeof si2151, "Si2151");
    build_profile(si2157, sizeof si2157, "Si2157");

    for (uint32_t layout = 3; layout <= 6; layout++)
        assert(oa_tuner_sal_type(layout, NULL) == OA_TUNER_SAL_EXTERNAL);
    assert(oa_tuner_sal_type(8, NULL) == OA_TUNER_SAL_SI2151);

    assert(oa_efsl_open(&p, si2151, sizeof si2151) == 0);
    assert(oa_tuner_sal_type(14, &p) == OA_TUNER_SAL_SI2151);
    assert(oa_tuner_sal_type(0, &p) == OA_TUNER_SAL_SI2151);
    assert(oa_tuner_sal_type(7, &p) == OA_TUNER_SAL_SI2151);

    assert(oa_efsl_open(&p, si2157, sizeof si2157) == 0);
    assert(oa_tuner_sal_type(14, &p) == OA_TUNER_SAL_MXL661);
    assert(oa_tuner_sal_type(0xffffffffu, &p) == OA_TUNER_SAL_MXL661);
    assert(oa_tuner_sal_type(14, NULL) == OA_TUNER_SAL_MXL661);

    /* _convertSalTunerType2NdNimTunerType table at 0x01355c74. */
    static const int expected[14] = {
        8, 8, 8, 0xc, 8, 5, 6, 5, 8, 8, 8, 8, 8, 0xc
    };
    for (int i = 0; i < 14; i++)
        assert(oa_tuner_ndnim_type(i) == expected[i]);
    assert(oa_tuner_ndnim_type(14) == 8);
    assert(oa_tuner_ndnim_type(-1) == 8);
    assert(oa_tuner_ndnim_type(0x7fffffff) == 8);
}

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    long n = ftell(f);
    assert(n > 0);
    rewind(f);
    uint8_t *buf = malloc((size_t)n);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)n, f) == (size_t)n);
    fclose(f);
    *size = (size_t)n;
    return buf;
}

/* The shipped profiles are private artifacts; the Makefile pins their MD5
 * before invoking this path. Counts/values below are the vendor file facts. */
static void test_corpus(const char *hw_path, const char *sw_path)
{
    size_t size;
    char val[32];
    size_t len = 0;
    oa_efsl_profile p;
    oa_efsl_section root, sec;

    uint8_t *hw = read_file(hw_path, &size);
    assert(oa_efsl_open(&p, hw, size) == 0);
    assert(oa_efsl_root(&p, &root) == 0 && root.count == 1214);
    assert(oa_efsl_open_section(&p, &root, "TunerTypes", &sec) == 0 &&
           sec.count == 5);
    assert(oa_efsl_read_value(&p, &sec, "TunerType", val, sizeof val, &len) ==
           0);
    assert(len == 7 && memcmp(val, "Si2157\0", 7) == 0);
    assert(oa_tuner_sal_type(14, &p) == OA_TUNER_SAL_MXL661);
    assert(oa_tuner_ndnim_type(OA_TUNER_SAL_MXL661) == OA_TUNER_NDNIM_MXL661);
    free(hw);

    if (sw_path) {
        uint8_t *sw = read_file(sw_path, &size);
        assert(oa_efsl_open(&p, sw, size) == 0);
        assert(oa_efsl_root(&p, &root) == 0 && root.count == 2992);
        assert(oa_efsl_open_section(&p, &root, "SwProfileVersion", &sec) == 0 &&
               sec.count == 1);
        assert(oa_efsl_read_value(&p, &sec, "Version", val, sizeof val, &len) ==
               0);
        assert(len == 3 && memcmp(val, "39\0", 3) == 0);
        free(sw);
    }
}

int main(int argc, char **argv)
{
    test_hash();
    test_parser();
    test_malformed();
    test_identity();
    if (argc > 1)
        test_corpus(argv[1], argc > 2 ? argv[2] : NULL);

    puts("ok efsl profile: hash/entry layout, readValue bounds, tuner identity "
         "and NdNim mapping");
    return 0;
}
