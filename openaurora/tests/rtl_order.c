/* Arabic/Persian visual order for the LTR card renderer. The expectations are
 * small hand-checkable strings plus the real langprofile's ara/per values. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/langprofile.h"

static void check(const char *in, const char *want)
{
    char out[512];
    assert(oa_langprofile_visual_order(in, out, sizeof out) == 0);
    if (strcmp(out, want)) {
        printf("visual order mismatch: in=%s got=%s want=%s\n", in, out, want);
        assert(0);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    check("\xef\xbb\xad\xef\xba\xbf\xef\xbb\x8a",
          "\xef\xbb\x8a\xef\xba\xbf\xef\xbb\xad");
    check("\xef\xba\x97\xef\xbb\xae\xef\xbb\x93\xef\xbb\xb4\xef\xba\xae\x20"
          "\xef\xba\x8d\xef\xbb\x9f\xef\xbb\x84\xef\xba\x8e\xef\xbb\x97"
          "\xef\xba\x94",
          "\xef\xba\x94\xef\xbb\x97\xef\xba\x8e\xef\xbb\x84\xef\xbb\x9f"
          "\xef\xba\x8d\x20\xef\xba\xae\xef\xbb\xb4\xef\xbb\x93\xef\xbb\xae"
          "\xef\xba\x97");
    check("\xef\xba\x97\xef\xbb\x9c\xef\xba\x92\xef\xbb\xb4\xef\xba\xae\x2f"
          "\xef\xba\x97\xef\xba\xbc\xef\xbb\x90\xef\xbb\xb4\xef\xba\xae\x20"
          "\xef\xba\x8d\xef\xbb\x9f\xef\xba\xbc\xef\xbb\xae\xef\xba\xad"
          "\xef\xba\x93",
          "\xef\xba\x93\xef\xba\xad\xef\xbb\xae\xef\xba\xbc\xef\xbb\x9f"
          "\xef\xba\x8d\x20\xef\xba\xae\xef\xbb\xb4\xef\xbb\x90\xef\xba\xbc"
          "\xef\xba\x97\x2f\xef\xba\xae\xef\xbb\xb4\xef\xba\x92\xef\xbb\x9c"
          "\xef\xba\x97");
    /* No Arabic code point: the English/Latin fallback stays LTR. */
    check("16:9", "16:9");
    check("Mode", "Mode");
    check("", "");
    /* Mixed runs: reverse the run order, keep Latin internally LTR. */
    check("ABC \xef\xbb\xad\xef\xba\xbf\xef\xbb\x8a",
          "\xef\xbb\x8a\xef\xba\xbf\xef\xbb\xad ABC");
    check("\xef\xbb\xad\xef\xba\xbf\xef\xbb\x8a ABC",
          "ABC \xef\xbb\x8a\xef\xba\xbf\xef\xbb\xad");

    char out[16];
    assert(oa_langprofile_visual_order(NULL, out, sizeof out) == EINVAL);
    assert(oa_langprofile_visual_order("abc", NULL, sizeof out) == EINVAL);
    assert(oa_langprofile_visual_order("abc", out, 0) == EINVAL);
    assert(oa_langprofile_visual_order("abcdef", out, 4) == EOVERFLOW);
    assert(oa_langprofile_visual_order("\xff", out, sizeof out) == EINVAL);

    struct oa_langprofile *p = NULL;
    assert(oa_langprofile_open(&p, argv[1]) == 0 && p);
    static const char *const keys[] = {
        "MODE", "ENERGY_SAVING", "PICTURE_ZOOM",
        "NATURAL", "MAXIMUM_STR", "ZOOM_MODE_16_9"
    };
    unsigned changed = 0;
    for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; i++) {
        for (unsigned l = 0; l < 2; l++) {
            const char *loc = l ? "per" : "ara";
            const char *stored = oa_langprofile_string(p, keys[i], loc);
            char clean[512], visual[512];
            assert(oa_langprofile_sanitize(stored, clean, sizeof clean) == 0);
            assert(oa_langprofile_visual_order(clean, visual,
                                               sizeof visual) == 0);
            int has_arabic = 0;
            for (const unsigned char *s = (const unsigned char *)clean;
                 *s; s++)
                if (s[0] >= 0xd8) has_arabic = 1;
            if (has_arabic) {
                assert(strcmp(clean, visual) != 0);
                changed++;
            } else {
                assert(!strcmp(clean, visual));
            }
        }
    }
    assert(changed == 10); /* all but the empty ara/per ZOOM_MODE_16_9 */
    oa_langprofile_close(p);
    puts("ok rtl order: Arabic/Persian run visual order and LTR fallbacks");
    return 0;
}
