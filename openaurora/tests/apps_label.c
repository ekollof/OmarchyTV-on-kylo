/* Apps-grid label placement: firmware centres the physical text width plus
 * first-bearing adjustment, not the ink bounding box. Synthetic font only. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/apps_label.h"

#define GLYPH_W 85
#define CELL_W 310

static unsigned char metrics[95 * 9];
static unsigned char coverage[81 + GLYPH_W];
static const unsigned char extremes[3] = { 0, 0, 1 };
static struct oa_bitmap_font font = {
    .pixels = coverage,
    .pixel_bytes = sizeof coverage,
    .metrics = metrics,
    .glyph_count = 95,
    .extremes = extremes
};

static void init_font(void)
{
    memset(metrics, 0, sizeof metrics);
    memset(coverage, 255, sizeof coverage);
    /* 'A': left bearing 4, no right bearing; ink width 81. */
    unsigned char *a = metrics + ('A' - 32) * 9;
    a[0] = 0; a[1] = 4; a[2] = 1; a[3] = GLYPH_W; a[4] = GLYPH_W;
    a[5] = 0; a[6] = 0; a[7] = 0; a[8] = 0;
    /* 'B': zero left bearing; ink width 85. Contrast case where the two
     * centring rules agree. */
    unsigned char *b = metrics + ('B' - 32) * 9;
    b[0] = 0; b[1] = 0; b[2] = 1; b[3] = GLYPH_W; b[4] = GLYPH_W;
    /* coverage offset 81, big-endian */
    b[5] = 0; b[6] = 0; b[7] = 0; b[8] = 81;
}

int main(void)
{
    init_font();
    unsigned char *rgba = NULL;
    int x = -1, w = -1, h = -1;

    /* Asymmetric left bearing: firmware origin = (310-85)/2 = 112, ink starts
     * at 112+4 = 116. Ink-box centring would give (310-81)/2 = 114. */
    assert(oa_apps_label_render(&font, "A", CELL_W, 8, &rgba, &x, &w, &h) == 0);
    assert(x == 116 && w == 81 && h == 1 && rgba);
    assert(rgba[0] == 38 && rgba[1] == 23 && rgba[2] == 25 && rgba[3] == 255);
    free(rgba);
    rgba = NULL;

    /* Zero left bearing: both rules place the ink at 112. This proves the
     * first case distinguishes the recovered rule instead of a constant. */
    assert(oa_apps_label_render(&font, "B", CELL_W, 8, &rgba, &x, &w, &h) == 0);
    assert(x == 112 && w == 85 && h == 1 && rgba);
    free(rgba);
    rgba = NULL;

    /* The caller's existing guard is preserved: over-wide text is rejected
     * before any allocation or drawing. */
    assert(oa_apps_label_render(&font, "AAAA", CELL_W, 8, &rgba, &x, &w, &h) ==
           -ERANGE);
    assert(rgba == NULL);

    assert(oa_apps_label_render(NULL, "A", CELL_W, 8, &rgba, &x, &w, &h) ==
           -EINVAL);
    assert(oa_apps_label_render(&font, NULL, CELL_W, 8, &rgba, &x, &w, &h) ==
           -EINVAL);
    assert(oa_apps_label_render(&font, "A", 0, 8, &rgba, &x, &w, &h) ==
           -EINVAL);
    assert(oa_apps_label_render(&font, "A", CELL_W, -1, &rgba, &x, &w, &h) ==
           -EINVAL);
    assert(oa_apps_label_render(&font, "A", CELL_W, 8, NULL, &x, &w, &h) ==
           -EINVAL);

    puts("ok apps label: firmware physical-width centring (asymmetric bearing "
         "+4 -> x=116, not ink-centred 114)");
    return 0;
}
