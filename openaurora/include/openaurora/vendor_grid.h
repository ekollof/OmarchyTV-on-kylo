/* SVGGrid placement arithmetic, recovered from this set's aurora.elf.
 *
 * The grid's own SVG description supplies the inputs; this module only performs
 * the arithmetic the firmware performs, with the same integer truncation.
 *
 *   SVGGrid::calculateHeight(SVGContainer*)          0x10c9d00
 *       +0x1f4 (top)    = mTopL height  (y1 - y0)
 *       +0x1f8 (bottom) = mBotL height
 *       +0x1e4 (row step) = mRowL height
 *
 *   SVGGrid::create(int id, EASIRect&, int cols, UIWindow*)   0x10cacb8
 *       +0x1e0 (cols) = cols
 *
 *   SVGGrid::renderBackground()                      0x10c9ed0
 *       +0x1dc (visible rows) = (h - top - bottom) / row step      [__aeabi_idiv]
 *       +0x1e8 (column step)  = (w - left - right) / cols
 *       +0x1ec (item height)  = rowPos height
 *
 *   SVGGrid::getButtonPosition(row, col, EASIRect&)  0x10ca8ac
 *       x = col * column step + left
 *       y = row * row step    + top
 *       x2 = x + column step + right
 *       y2 = y + item height
 *
 * left (+0x1fc) and right (+0x200) are runtime fields, not description constants:
 * renderBackground rewrites them inside its per-cell loop from the rowPos
 * rectangle and the horizontal mask rectangles, and createListButtons calls
 * getButtonPosition afterwards. Their value therefore depends on the cell that
 * was painted last. This module applies the formula only; caller-supplied values
 * must be the ones the firmware has at that moment (a freshly constructed grid
 * has zeroes). Do not assume zero for a painted grid.
 */
#ifndef OPENAURORA_VENDOR_GRID_H
#define OPENAURORA_VENDOR_GRID_H
typedef struct {
    int w, h;       /* grid window rect */
    int cols;       /* +0x1e0 */
    int top;        /* +0x1f4, mTopL height */
    int bottom;     /* +0x1f8, mBotL height */
    int row_step;   /* +0x1e4, mRowL height */
    int item_h;     /* +0x1ec, rowPos height */
    int left;       /* +0x1fc */
    int right;      /* +0x200 */
} oa_vgrid;

int oa_vgrid_visible_rows(const oa_vgrid *g);
int oa_vgrid_col_step(const oa_vgrid *g);
void oa_vgrid_button(const oa_vgrid *g, int row, int col, int *x, int *y, int *x2, int *y2);

#endif
