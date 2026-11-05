/* SVGGrid placement arithmetic; see include/openaurora/vendor_grid.h. */
#include "openaurora/vendor_grid.h"

/* renderBackground stores the quotient of __aeabi_idiv, i.e. C integer
 * division truncated toward zero. Negative spans are not produced by the
 * firmware (the window and margins come from the same description), but guard
 * the two divisors so a malformed description cannot trap. */
int oa_vgrid_visible_rows(const oa_vgrid *g)
{
    if (!g || g->row_step <= 0)
        return 0;
    return (g->h - g->top - g->bottom) / g->row_step;
}

int oa_vgrid_col_step(const oa_vgrid *g)
{
    if (!g || g->cols <= 0)
        return 0;
    return (g->w - g->left - g->right) / g->cols;
}

void oa_vgrid_cell(const oa_vgrid *g, int row, int col, int *x, int *y, int *x2, int *y2)
{
    int step;
    if (!g) {
        if (x) *x = 0;
        if (y) *y = 0;
        if (x2) *x2 = 0;
        if (y2) *y2 = 0;
        return;
    }
    step = oa_vgrid_col_step(g);
    if (x) *x = col * step;
    if (y) *y = row * g->row_step + g->top;
    if (x2) *x2 = col * step + step;
    if (y2) *y2 = row * g->row_step + g->top + g->row_step;
}

void oa_vgrid_button(const oa_vgrid *g, int row, int col, int *x, int *y, int *x2, int *y2)
{
    int step;
    if (!g) {
        if (x) *x = 0;
        if (y) *y = 0;
        if (x2) *x2 = 0;
        if (y2) *y2 = 0;
        return;
    }
    step = oa_vgrid_col_step(g);
    if (x) *x = col * step + g->left;
    if (y) *y = row * g->row_step + g->top;
    /* The shipped getter adds +0x1fc both to x and again to x2. */
    if (x2) *x2 = col * step + g->left + g->left + step + g->right;
    if (y2) *y2 = row * g->row_step + g->top + g->item_h;
}

/* DialogFullScreenPortal.svg (114), 1920x1080 design space. */
void oa_portal_plan_init(oa_portal_plan *p)
{
    if (!p)
        return;
    p->screen = (oa_vrect){ 0, 0, 1920, 1080 };
    p->top_strip = (oa_vrect){ 0, 0, 1921, 162 };
    p->apps_grid = (oa_vrect){ 183, 180, 1554, 720 };
    p->scrollbar = (oa_vrect){ 1750, 180, 8, 720 };
    p->title = (oa_vrect){ 7, 39, 546, 90 };
    p->logo = (oa_vrect){ 1587, 32, 300, 120 };
    p->paging = (oa_vrect){ 893, 984, 223, 60 };
    p->portal_mode = (oa_vrect){ 1474, 984, 250, 60 };
    /* FullScreenGridFA.svg: mRowL height 230, rowPos height 230, no mTopL/mBotL
     * rectangles; SVGGrid::create arg 5 is the column count. The named-rectangle
     * transform maps rowPos onto each cell surface, so the runtime left/right
     * fields reduce to the zero-width horizontal masks. */
    p->grid = (oa_vgrid){ 1554, 720, 5, 0, 0, 230, 230, 0, 0 };
}
