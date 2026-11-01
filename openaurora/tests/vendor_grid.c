/* Verifies the recovered SVGGrid arithmetic against the vendor's own numbers.
 * Inputs are the rectangles dumped from the BinSVG pack:
 *   FullScreenGridFA.svg (375): mRowL (156,122,0,230), rowPos (225,121,320,230)
 *   gridFA.svg (18):           mTopL 16 high, mBotL 10 high, mRowL 69 high
 * The portal window (appsGrid) is (183,180,1554,720) in DialogFullScreenPortal.svg. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "openaurora/vendor_grid.h"

int main(void)
{
    /* DialogFullScreenPortal: 5 columns (SVGGrid::create arg 5), no mTopL/mBotL in
     * FullScreenGridFA, so top/bottom are zero; row step and rowPos height are 230. */
    oa_vgrid portal = { 1554, 720, 5, 0, 0, 230, 230, 0, 0 };
    assert(oa_vgrid_visible_rows(&portal) == 3);
    assert(oa_vgrid_col_step(&portal) == 310);

    int x, y, w, h;
    oa_vgrid_button(&portal, 0, 0, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 310 && h == 230);
    oa_vgrid_button(&portal, 2, 4, &x, &y, &w, &h);
    assert(x == 1240 && y == 460 && w == 1550 && h == 690);

    /* gridFA margins: rows = (720 - 16 - 10) / 69 = 694 / 69 = 10 (truncating),
     * column step = (1280 - 8 - 8) / 3 = 1264 / 3 = 421. */
    oa_vgrid grid = { 1280, 720, 3, 16, 10, 69, 69, 8, 8 };
    assert(oa_vgrid_visible_rows(&grid) == 10);
    assert(oa_vgrid_col_step(&grid) == 421);
    oa_vgrid_button(&grid, 2, 2, &x, &y, &w, &h);
    assert(x == 850 && y == 154 && w == 1279 && h == 223);

    /* Truncation is per-cell, not accumulated from the unrounded pitch:
     * step = 94 / 4 = 23, so column 3 starts at 3 * 23 + 3 = 72. */
    oa_vgrid trunc = { 100, 100, 4, 10, 9, 20, 25, 3, 3 };
    assert(oa_vgrid_visible_rows(&trunc) == 4);
    assert(oa_vgrid_col_step(&trunc) == 23);
    oa_vgrid_button(&trunc, 3, 3, &x, &y, &w, &h);
    assert(x == 72 && y == 70 && w == 98 && h == 95);

    /* Divisors come from the description; guard them instead of trapping. */
    oa_vgrid zero = { 100, 100, 0, 0, 0, 0, 0, 0, 0 };
    assert(oa_vgrid_visible_rows(&zero) == 0 && oa_vgrid_col_step(&zero) == 0);
    oa_vgrid_button(&zero, 1, 1, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);
    oa_vgrid_button(NULL, 1, 1, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);

    puts("  ok   SVGGrid: rows/column step truncate like __aeabi_idiv; button origins and sizes follow the firmware formula");
    puts("  ok   portal appsGrid (5 cols, 1554x720, row step 230) yields 3 rows and a 310 px column step; fresh-grid zero insets give 310x230 buttons");
    puts("  ok   gridFA margins reproduce mTopL/mBotL/mRowL; runtime +0x1fc/+0x200 are per-cell paint state, not tested as constants");
    return 0;
}
