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
    if (x2) *x2 = col * step + g->left + step + g->right;
    if (y2) *y2 = row * g->row_step + g->top + g->item_h;
}
