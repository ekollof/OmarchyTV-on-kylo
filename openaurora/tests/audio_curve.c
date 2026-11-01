#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "openaurora/audio_curve.h"

static void read_exact(const char *path, void *data, size_t size)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fread(data, 1, size, file) == size);
    assert(fgetc(file) == EOF && !ferror(file));
    assert(!fclose(file));
}
int main(int argc, char **argv)
{
    assert(argc == 1 || argc == 3);
    uint8_t storage[OA_AUDIO_AQ_SIZE + 1] = {0};
    uint8_t *aq = storage + 1; /* Deliberately unaligned. */
    for (unsigned i = 0; i < OA_AUDIO_CURVE_POINTS; i++) {
        size_t normal = 0x24 + 4 * i, aggressive = 0x1b8 + 4 * i;
        aq[normal] = (uint8_t)i; aq[normal + 1] = 0x12;
        aq[normal + 2] = 0xfe; aq[normal + 3] = 0xff;
        aq[aggressive] = (uint8_t)(100 - i); aq[aggressive + 1] = 0xab;
        aq[aggressive + 2] = 0xdc; aq[aggressive + 3] = 0x98;
    }
    oa_audio_curve curve, before;
    const int profiles[] = {INT_MIN, -1, 0, 1, 2, INT_MAX};
    for (size_t p = 0; p < sizeof profiles / sizeof profiles[0]; p++) {
        assert(!oa_audio_curve_from_aq(&curve, aq, OA_AUDIO_AQ_SIZE, profiles[p]));
        for (unsigned i = 0; i < OA_AUDIO_CURVE_POINTS; i++) {
            uint16_t expected = profiles[p] == 1 ? (uint16_t)(0xab00 + 100 - i) : (uint16_t)(0x1200 + i);
            uint16_t value = 0;
            assert(curve.points[i] == expected);
            assert(!oa_audio_curve_value(&curve, i, &value) && value == expected);
        }
    }
    puts("  ok   both 101-entry AQ tables: exact selection, unaligned little-endian loads and 32-to-16-bit narrowing");
    memset(&curve, 0x5a, sizeof curve); before = curve;
    for (size_t size = 0; size < OA_AUDIO_AQ_SIZE; size++) {
        assert(oa_audio_curve_from_aq(&curve, aq, size, 0) == -1);
        assert(!memcmp(&curve, &before, sizeof curve));
    }
    assert(oa_audio_curve_from_aq(&curve, aq, OA_AUDIO_AQ_SIZE + 1, 0) == -1);
    assert(oa_audio_curve_from_aq(&curve, aq, SIZE_MAX, 0) == -1);
    assert(oa_audio_curve_from_aq(&curve, NULL, OA_AUDIO_AQ_SIZE, 0) == -1);
    assert(oa_audio_curve_from_aq(NULL, aq, OA_AUDIO_AQ_SIZE, 0) == -1);
    assert(!memcmp(&curve, &before, sizeof curve));
    uint16_t value = 123;
    assert(oa_audio_curve_value(NULL, 0, &value) == -1 && value == 123);
    assert(oa_audio_curve_value(&curve, 0, NULL) == -1);
    assert(!oa_audio_curve_from_aq(&curve, aq, OA_AUDIO_AQ_SIZE, 0));
    assert(!oa_audio_curve_value(&curve, 101, &value) && value == 0x1264);
    assert(!oa_audio_curve_value(&curve, UINT_MAX, &value) && value == 0x1264);
    puts("  ok   every truncated length / oversized inputs reject atomically; null guards and unsigned volume clamping");
    if (argc == 3) {
        uint8_t saved[OA_AUDIO_CURVE_POINTS * 2];
        read_exact(argv[1], aq, OA_AUDIO_AQ_SIZE);
        read_exact(argv[2], saved, sizeof saved);
        assert(!oa_audio_curve_from_aq(&curve, aq, OA_AUDIO_AQ_SIZE, 0));
        for (unsigned i = 0; i < OA_AUDIO_CURVE_POINTS; i++)
            assert(curve.points[i] == (unsigned)saved[2*i] + ((unsigned)saved[2*i+1] << 8));
        assert(curve.points[0] == 0 && curve.points[1] == 5 && curve.points[50] == 8137 && curve.points[100] == 32768);
        puts("  ok   all 101 decoded normal-curve entries match the independently captured real-TV table");
    }
    return 0;
}
