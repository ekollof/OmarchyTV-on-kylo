/* Locale audit for the sub-launcher live text: resolve all 41 dictionary
 * locales, measure the actual font3/font12 widths, and report any locale
 * whose string does not fit the 235 px card or lacks glyphs. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/font_bitmap.h"
#include "openaurora/font_pack.h"
#include "openaurora/font_text.h"
#include "openaurora/langprofile.h"

struct font {
    struct oa_bitmap_font bitmap;
    unsigned char *data;
};

static int open_font(struct oa_font_pack *pack, int id, struct font *out)
{
    size_t size = 0;
    if (oa_font_pack_read(pack, (unsigned)id, &out->data, &size)) return -1;
    if (oa_bitmap_font_open(&out->bitmap, out->data, size)) {
        free(out->data);
        out->data = NULL;
        return -1;
    }
    return 0;
}

static int advance(const struct oa_bitmap_font *font, const char *text)
{
    int width = 0;
    if (oa_font_text_measure(font, text, 0, &width)) return -1;
    return width;
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    struct oa_langprofile *profile = NULL;
    assert(oa_langprofile_open(&profile, argv[1]) == 0 && profile);
    struct oa_font_pack pack;
    assert(oa_font_pack_init(&pack, argv[2], argv[3]) == 0);
    struct font label, value;
    assert(open_font(&pack, 3, &label) == 0);
    assert(open_font(&pack, 12, &value) == 0);
    assert(oa_langprofile_locale_count(profile) == 41);

    static const char *const keys[] = {
        "MODE", "ENERGY_SAVING", "PICTURE_ZOOM",
        "NATURAL", "MAXIMUM_STR", "ZOOM_MODE_16_9"
    };
    unsigned missing = 0, label_overflow = 0, value_overflow = 0;
    int max_label = 0, max_value = 0;
    for (unsigned i = 0; i < oa_langprofile_locale_count(profile); i++) {
        const char *locale = oa_langprofile_locale(profile, i);
        for (unsigned k = 0; k < sizeof keys / sizeof keys[0]; k++) {
            const char *text = oa_langprofile_string(profile, keys[k], locale);
            assert(text && text[0]);
            char clean[256];
            assert(oa_langprofile_sanitize(text, clean, sizeof clean) == 0);
            int w = advance(k < 3 ? &label.bitmap : &value.bitmap, clean);
            if (w < 0) {
                printf("missing glyph: %s %s=%s\n", locale, keys[k], text);
                missing++;
                continue;
            }
            if (k < 3) {
                if (w > max_label) max_label = w;
                if (w > 235) {
                    label_overflow++;
                    if (label_overflow <= 8)
                        printf("label overflow: %s %s %d px\n", locale,
                               keys[k], w);
                }
            } else {
                if (w > max_value) max_value = w;
                if (w > 235) {
                    value_overflow++;
                    if (value_overflow <= 8)
                        printf("value overflow: %s %s %d px\n", locale,
                               keys[k], w);
                }
            }
        }
    }
    assert(!strcmp(oa_langprofile_string(profile, "MODE", "dut"), "Modus"));
    assert(!strcmp(oa_langprofile_string(profile, "MODE", "eng"), "Mode"));
    assert(!strcmp(oa_langprofile_string(profile, "ZOOM_MODE_16_9", "dut"),
                   "16:9"));
    /* Hebrew carries U+202C directional controls; stripping them makes the
     * existing Hebrew glyphs measurable. This is not BiDi reordering. */
    const char *heb = oa_langprofile_string(profile, "ENERGY_SAVING", "heb");
    char heb_clean[256];
    assert(oa_langprofile_sanitize(heb, heb_clean, sizeof heb_clean) == 0);
    assert(!strstr(heb_clean, "\xe2\x80\xac"));
    assert(advance(&label.bitmap, heb_clean) > 0);
    printf("ok sub text locales: 41 locales; max label %d px, max value %d px; "
           "%u label/%u value overflow, %u missing glyphs\n",
           max_label, max_value, label_overflow, value_overflow, missing);
    oa_font_pack_free(&pack);
    free(label.data);
    free(value.data);
    oa_langprofile_close(profile);
    return 0;
}
