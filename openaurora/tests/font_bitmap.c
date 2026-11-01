#include "openaurora/font_bitmap.h"
#include "openaurora/font_finder.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void synthetic(void)
{
    /* 95 ASCII + two packed UTF8 keys; one grayscale pixel per glyph. */
    unsigned char b[8 + 1 + 5 + 8 + 5 + 98 * 9] = {0};
    memcpy(b, "EF41", 4); b[7] = 1; b[8] = 213;
    memcpy(b + 9, "UTF", 3); b[13] = 2;
    b[16] = 0xc2; b[17] = 0xa0; b[20] = 0xc3; b[21] = 0xa9;
    memcpy(b + 22, "DIM", 3); b[26] = 97;
    for (unsigned i = 0; i < 97; ++i) {
        unsigned char *m = b + 27 + i * 9;
        m[0] = 255; m[1] = 255; m[4] = 3;
    }
    struct oa_bitmap_font f;
    struct oa_bitmap_glyph g;
    unsigned index;
    assert(!oa_bitmap_font_open(&f, b, sizeof b));
    assert(!oa_bitmap_font_lookup(&f, '!', &index) && index == 1);
    assert(!oa_bitmap_font_lookup(&f, 0xc3a9, &index) && index == 96);
    assert(oa_bitmap_font_lookup(&f, 0xe9, &index) == ENOENT);
    assert(!oa_bitmap_font_glyph(&f, 96, &g));
    assert(g.left == -1 && g.top == -1 && g.stride == 1 &&
           g.advance == 3 && g.coverage[0] == 213);
    assert(oa_bitmap_font_glyph(&f, 97, &g) == EINVAL);
    for (size_t n = 0; n < sizeof b; ++n) {
        assert(oa_bitmap_font_open(&f, b, n));
        assert(!f.metrics);
    }
    b[3] = '0'; assert(oa_bitmap_font_open(&f, b, sizeof b) == ENOTSUP); b[3] = '1';
    b[17] = 0xff; b[16] = 0xff;
    assert(oa_bitmap_font_open(&f, b, sizeof b) == EINVAL);
    b[16] = 0xc2; b[17] = 0xa0;
    b[35] = 1; /* offset1 + area1 exceeds bitmap */
    assert(oa_bitmap_font_open(&f, b, sizeof b) == EINVAL); b[35] = 0;
    b[30] = 254; /* right -2 < left -1 */
    assert(!oa_bitmap_font_open(&f, b, sizeof b));
    assert(!oa_bitmap_font_glyph(&f, 0, &g) && !g.stride && g.right == -2);
    puts("ok bitmap font: mapping, signed metrics, every truncation, corrupt bounds/order");
}

/* Optional local firmware corpus validation. Vendor bytes are never embedded. */
static int corpus(const char *pack_path, const char *index_path)
{
    int heights[39] = {0};
    FILE *pack = fopen(pack_path, "rb"), *idx = fopen(index_path, "r");
    if (!pack || !idx) { perror("font pack/index"); return 1; }
    unsigned count;
    if (fscanf(idx, "%u", &count) != 1 || count != 39) return 1;
    for (unsigned i = 0; i < count; ++i) {
        unsigned length;
        if (fscanf(idx, "%u", &length) != 1 || !length || length > 32000000) return 1;
        unsigned char *bytes = malloc(length);
        if (!bytes || fread(bytes, 1, length, pack) != length) return 1;
        if (!i) {
            if (length != 1 || bytes[0]) return 1;
            puts("font0 default-font sentinel");
        } else {
            struct oa_bitmap_font f;
            int rc = oa_bitmap_font_open(&f, bytes, length);
            if (rc) { fprintf(stderr, "font%u rejected: %d\n", i, rc); return 1; }
            if (oa_bitmap_font_height(&f, &heights[i])) return 1;
            /* Every stored key must resolve to exactly its metric slot. */
            for (unsigned k = 0; k < f.key_count; ++k) {
                const unsigned char *p = f.keys + (size_t)k * 4;
                uint32_t key = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
                               (uint32_t)p[2] << 8 | p[3];
                unsigned slot;
                if (oa_bitmap_font_lookup(&f, key, &slot) || slot != k + 95) return 1;
            }
            printf("font%u bytes=%u pixels=%zu glyphs=%u keys=%u height=%d\n",
                   i, length, f.pixel_bytes, f.glyph_count, f.key_count, heights[i]);
        }
        free(bytes);
    }
    int rc = fgetc(pack) != EOF || ferror(pack);
    fclose(pack); fclose(idx);
    /* DemoApp::addFonts cc52e0 registration order, not record-number order. */
    static const unsigned ids[] = {18,17,16,7,6,5,4,3,2,1,22,21,20,14,13,12,11,10,9,8};
    static const int expected[] = {80,75,68,61,56,50,43,35,30,24,81,74,67,63,56,50,42,34,24,19};
    struct oa_font_finder finder = {0};
    for (unsigned n = 0; n < sizeof ids / sizeof *ids; ++n) {
        if (heights[ids[n]] != expected[n] ||
            oa_font_finder_add(&finder, ids[n], heights[ids[n]], n / 10)) return 1;
    }
    static const struct { int height, lines; unsigned type, expected; } cases[] = {
        {23,1,0,8}, {24,1,0,1}, {48,1,0,4}, {48,1,1,11},
        {66,1,0,7}, {66,1,1,14}, {100,2,0,5}, {100,2,1,12},
        {100,1,0,18}, {100,1,1,22}, {48,1,5,4}, {48,1,99,4}
    };
    for (unsigned n = 0; n < sizeof cases / sizeof *cases; ++n) {
        unsigned selected;
        if (oa_font_finder_select(&finder, cases[n].height, cases[n].lines,
                                 cases[n].type, &selected) || selected != cases[n].expected)
            return 1;
    }
    puts("ok theme font registration: 20 heights, typed sizes/multiline/global fallback");
    return rc;
}

int main(int argc, char **argv)
{
    synthetic();
    if (argc == 3) return corpus(argv[1], argv[2]);
    return argc != 1;
}
