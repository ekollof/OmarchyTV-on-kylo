#include "openaurora/canvas.h"
#include "openaurora/font_bitmap.h"
#include "openaurora/font_pack.h"
#include "openaurora/font_text.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 95 ASCII glyphs, 1x1 coverage, advance 3; coverage = 10+index. */
static size_t build_font(unsigned char *b)
{
    const unsigned glyphs = 95;
    memcpy(b, "EF41", 4);
    b[4] = b[5] = b[6] = 0;
    b[7] = glyphs;
    for (unsigned i = 0; i < glyphs; ++i) b[8 + i] = (unsigned char)(10 + i);
    size_t at = 8 + glyphs;
    memcpy(b + at, "UTF", 3);
    b[at + 3] = b[at + 4] = 0;
    at += 5;
    memcpy(b + at, "DIM", 3);
    b[at + 3] = 0;
    b[at + 4] = glyphs;
    at += 5;
    for (unsigned i = 0; i < glyphs; ++i) {
        unsigned char *m = b + at + i * 9;
        m[0] = 0; m[1] = 0; m[2] = 1; m[3] = 1; m[4] = 3;
        m[5] = 0; m[6] = 0; m[7] = 0; m[8] = (unsigned char)i;
    }
    unsigned char *x = b + at + glyphs * 9;
    memset(x, 0, 9);
    return at + glyphs * 9 + 9;
}

static int corpus(const char *index_path, const char *pack_path)
{
    struct oa_font_pack pack;
    struct oa_bitmap_font font;
    unsigned char *data;
    size_t size;
    assert(!oa_font_pack_init(&pack, index_path, pack_path));
    assert(!oa_font_pack_read(&pack, 7, &data, &size));
    assert(!oa_bitmap_font_open(&font, data, size));
    int height, width;
    assert(!oa_bitmap_font_height(&font, &height) && height == 61);
    assert(!oa_font_text_measure(&font, "Modus", 0, &width) && width == 118);
    assert(!oa_font_text_measure(&font, "Contrast", 0, &width) && width == 143);
    assert(!oa_font_text_measure(&font, "Energiebesparing", 0, &width) && width == 302);
    puts("  ok   real font7 measures the captured settings labels (118/143/302)");
    free(data);
    oa_font_pack_free(&pack);
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char b[1024];
    size_t size = build_font(b);
    struct oa_bitmap_font font;
    assert(!oa_bitmap_font_open(&font, b, size));
    int width;
    assert(!oa_font_text_measure(&font, "AB", 0, &width) && width == 6);
    assert(!oa_font_text_measure(&font, "AB", 1, &width) && width == 8);
    assert(!oa_font_text_measure(&font, "", 0, &width) && width == 0);

    unsigned char pixels[8 * 2 * 4] = {0};
    oa_canvas canvas = { pixels, 8, 2 };
    const uint8_t white[4] = {255, 255, 255, 255};
    assert(!oa_font_text_draw(&canvas, &font, "AB", 0, 0, white, 0, NULL));
    assert(pixels[0] == 255 && pixels[1] == 255 && pixels[2] == 255);
    assert(pixels[3] == 43 && pixels[7] == 0);               /* 'A' coverage */
    assert(pixels[3 * 4 + 3] == 44 && pixels[3 * 4 + 7] == 0);   /* 'B' advance 3 */

    memset(pixels, 0, sizeof pixels);
    struct oa_font_clip clip = {0, 0, 1, 2};
    assert(!oa_font_text_draw(&canvas, &font, "AB", 0, 0, white, 0, &clip));
    assert(pixels[3] == 43 && pixels[3 * 4 + 3] == 0);         /* clipped 'B' */

    memset(pixels, 0, sizeof pixels);
    assert(oa_font_text_draw(&canvas, &font, "\xc3\xa9", 0, 0, white, 0, NULL) == ENOENT);
    for (size_t i = 0; i < sizeof pixels; ++i) assert(pixels[i] == 0);
    assert(oa_font_text_draw(&canvas, &font, "\xff", 0, 0, white, 0, NULL) == EILSEQ);
    assert(oa_font_text_draw(&canvas, &font, "AB", 0, 0, white, 0, NULL) == 0);

    memset(pixels, 0, sizeof pixels);
    assert(!oa_canvas_clear(&canvas, 0, 0, 0, 255));
    const uint8_t translucent[4] = {255, 255, 255, 128};
    assert(!oa_font_text_draw(&canvas, &font, "A", 0, 0, translucent, 0, NULL));
    assert(pixels[0] == 21 && pixels[1] == 21 && pixels[2] == 21 && pixels[3] == 255);

    puts("ok font text: UTF-8 keys, advances/spacing, clips, missing/invalid rejection, source-over");
    if (argc == 3) return corpus(argv[1], argv[2]);
    return 0;
}
