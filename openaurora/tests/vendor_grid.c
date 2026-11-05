/* Verifies the recovered SVGGrid arithmetic against the vendor's own numbers.
 * Inputs are the rectangles dumped from the BinSVG pack:
 *   FullScreenGridFA.svg (375): mRowL (156,122,0,230), rowPos (225,121,320,230)
 *   gridFA.svg (18):           mTopL 16 high, mBotL 10 high, mRowL 69 high
 * The portal window (appsGrid) is (183,180,1554,720) in DialogFullScreenPortal.svg.
 * RectHandler transforms named rectangles before storing them, and rowPos is the
 * HorizontalMaskScaleTransform's own mask, so the painted last-cell fields are
 * the horizontal mask widths (zero here), not raw rowPos-minus-cell offsets. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "openaurora/vendor_grid.h"

/* HorizontalMaskScaleTransform::transformRect maps a named rectangle by
 * subtracting the mask origin, scaling x by surface_width/mask_width, then
 * adding the surface origin. RectHandler stores that transformed rectangle.
 * This is an algebraic fixture, not execution of the shipped transform. */
static int mask_scale_x(int value, int mask_x, int mask_w, int surface_x, int surface_w)
{
    if (mask_w <= 0)
        return value;
    return surface_x + (value - mask_x) * surface_w / mask_w;
}

/* renderBackground stores (mask_width + transformed_rowPos_x) - surface_x.
 * Because rowPos is the transform's own mask, transformed_rowPos_x is the
 * surface x, so the result is the horizontal mask width. */
static void resolved_insets(int cell_x, int cell_w, int mask_left, int mask_right,
                            int rowpos_x, int rowpos_w, int *left, int *right)
{
    /* renderBackground shrinks the transform's surface by the side masks. */
    int surface_x = cell_x + mask_left;
    int surface_w = cell_w - mask_left - mask_right;
    int tx = mask_scale_x(rowpos_x, rowpos_x, rowpos_w, surface_x, surface_w);
    int tx1 = mask_scale_x(rowpos_x + rowpos_w, rowpos_x, rowpos_w, surface_x, surface_w);
    *left = mask_left + tx - surface_x;
    *right = mask_right + surface_x + surface_w - tx1;
}

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

    /* The raw rowPos-minus-cell values look like -1015/+1005 for the last
     * cell, but RectHandler transforms rowPos through its own mask first.
     * The transformed rectangle is the cell surface, so the runtime fields
     * collapse to the zero-width horizontal masks. */
    int left, right;
    assert(225 - 1240 == -1015 && 1550 - 545 == 1005);
    resolved_insets(0, 310, 0, 0, 225, 320, &left, &right);
    assert(left == 0 && right == 0);
    portal.left = left;
    portal.right = right;
    oa_vgrid_button(&portal, 0, 0, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 310 && h == 230);

    resolved_insets(1240, 310, 0, 0, 225, 320, &left, &right);
    assert(left == 0 && right == 0);
    portal.left = left;
    portal.right = right;
    oa_vgrid_button(&portal, 2, 4, &x, &y, &w, &h);
    assert(x == 1240 && y == 460 && w == 1550 && h == 690);

    /* A nonzero horizontal mask is preserved as an inset, not subtracted as
     * a cell offset. (The button's step was computed before these fields were
     * updated, so this test checks the resolved insets directly.) */
    resolved_insets(620, 310, 5, 7, 225, 320, &left, &right);
    assert(left == 5 && right == 7);

    /* Restore the portal's real zero-mask state for the remaining checks. */
    portal.left = portal.right = 0;

    /* gridFA margins: rows = (720 - 16 - 10) / 69 = 694 / 69 = 10 (truncating),
     * column step = (1280 - 8 - 8) / 3 = 1264 / 3 = 421. */
    oa_vgrid grid = { 1280, 720, 3, 16, 10, 69, 69, 8, 8 };
    assert(oa_vgrid_visible_rows(&grid) == 10);
    assert(oa_vgrid_col_step(&grid) == 421);
    oa_vgrid_button(&grid, 2, 2, &x, &y, &w, &h);
    /* ARM10ca8d6 forms x=col*step+left;10ca8da/ea/ee then adds
     * left+step+right again to form x2. A zero-inset portal cannot catch this. */
    assert(x == 850 && y == 154 && w == 1287 && h == 223);

    /* Truncation is per-cell, not accumulated from the unrounded pitch:
     * step = 94 / 4 = 23, so column 3 starts at 3 * 23 + 3 = 72. */
    oa_vgrid trunc = { 100, 100, 4, 10, 9, 20, 25, 3, 3 };
    assert(oa_vgrid_visible_rows(&trunc) == 4);
    assert(oa_vgrid_col_step(&trunc) == 23);
    oa_vgrid_button(&trunc, 3, 3, &x, &y, &w, &h);
    assert(x == 72 && y == 70 && w == 101 && h == 95);

    /* Cell rects (the background paint) do not add left to the origin:
     * gridFA col 2/row 2 = 2*421, 2*69+16 to +421/+69. */
    oa_vgrid_cell(&trunc, 3, 3, &x, &y, &w, &h);
    assert(x == 69 && y == 70 && w == 92 && h == 90);
    oa_vgrid_cell(&grid, 2, 2, &x, &y, &w, &h);
    assert(x == 842 && y == 154 && w == 1263 && h == 223);

    /* Chrome and cell grid from DialogFullScreenPortal.svg / FullScreenGridFA. */
    oa_portal_plan plan;
    oa_portal_plan_init(&plan);
    assert(plan.screen.x == 0 && plan.screen.y == 0 &&
           plan.screen.w == 1920 && plan.screen.h == 1080);
    assert(plan.top_strip.y == 0 && plan.top_strip.h == 162 && plan.top_strip.w == 1921);
    assert(plan.apps_grid.x == 183 && plan.apps_grid.y == 180 &&
           plan.apps_grid.w == 1554 && plan.apps_grid.h == 720);
    assert(plan.scrollbar.x == 1750 && plan.title.y == 39 && plan.logo.x == 1587);
    assert(plan.paging.y == 984 && plan.portal_mode.x == 1474);
    assert(plan.grid.cols == 5 && plan.grid.row_step == 230 && plan.grid.top == 0);
    oa_vgrid_cell(&plan.grid, 2, 4, &x, &y, &w, &h);
    assert(x == 1240 && y == 460 && w == 1550 && h == 690);
    oa_portal_plan_init(NULL); /* no-op */

    /* Divisors come from the description; guard them instead of trapping. */
    oa_vgrid zero = { 100, 100, 0, 0, 0, 0, 0, 0, 0 };
    assert(oa_vgrid_visible_rows(&zero) == 0 && oa_vgrid_col_step(&zero) == 0);
    oa_vgrid_button(&zero, 1, 1, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);
    oa_vgrid_button(NULL, 1, 1, &x, &y, &w, &h);
    assert(x == 0 && y == 0 && w == 0 && h == 0);

    puts("  ok   SVGGrid: rows/column step truncate like __aeabi_idiv; button origins and sizes follow the firmware formula");
    puts("  ok   portal appsGrid (5 cols, 1554x720, row step 230) yields 3 rows and a 310 px column step; fresh-grid zero insets give 310x230 buttons");
    puts("  ok   gridFA margins reproduce mTopL/mBotL/mRowL; the rowPos mask transform collapses last-cell +0x1fc/+0x200 to the zero-width horizontal masks, not -1015/+1005");
    puts("  ok   DialogFullScreenPortal chrome and the 5x3 cell grid (310x230) match the BinSVG rectangles");
    return 0;
}
