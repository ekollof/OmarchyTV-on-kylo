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
 *       x2 = x + left + column step + right
 *       y2 = y + item height
 *
 * left (+0x1fc) and right (+0x200) are written inside renderBackground's
 * per-cell loop, then read by getButtonPosition. The subtraction is not
 * rowPos-minus-cell: RectHandler applies the active HorizontalMaskScaleTransform
 * to every named rectangle before adding it to the RectangleContainer. The
 * transform's mask name is rowPos itself, so the stored rowPos rectangle is
 * mapped onto the cell surface. The subtractions therefore reduce to the
 * horizontal mask widths. For this portal the relevant horizontal masks are
 * zero-width (mTopL/mBotL are path/polyline elements with no rectangle;
 * mRowL/mRowR rectangles are zero-width), so the painted grid's insets are
 * zero, not the -1015/+1005 raw rowPos-minus-cell values. A nonzero mask width
 * is still a valid caller-supplied state; this module applies the formula only.
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
/* Cell rectangle painted by renderBackground: (col*step, row*row_step+top) to
 * (+step, +row_step). It does not add left/right to the origin, but the step
 * itself is (w-left-right)/cols. */
void oa_vgrid_cell(const oa_vgrid *g, int row, int col, int *x, int *y, int *x2, int *y2);
/* Button rectangle from getButtonPosition; caller supplies the runtime
 * left/right fields (see the comment above). */
void oa_vgrid_button(const oa_vgrid *g, int row, int col, int *x, int *y, int *x2, int *y2);

/* Static chrome of DialogFullScreenPortal.svg (114), 1920x1080:
 * background, top strip, appsGrid, scrollbar, title, logo, paging and
 * portalMode. The cell grid is the five-column FullScreenGrid from
 * FullScreenGridFA.svg; left/right are the fresh-grid zeroes and are NOT the
 * painted runtime state. */
typedef struct {
    int x, y, w, h;
} oa_vrect;

typedef struct {
    oa_vrect screen, top_strip, apps_grid, scrollbar, title, logo, paging, portal_mode;
    oa_vgrid grid;
} oa_portal_plan;

void oa_portal_plan_init(oa_portal_plan *p);

#endif
