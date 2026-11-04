#ifndef OA_SVG_SCROLLBAR_H
#define OA_SVG_SCROLLBAR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SVGScrollBar state/geometry, recovered from the shipped Thumb bodies:
 *   setScrollPosition          0x10c4138 (0x70 bytes, CRC 0x71cda0c0)
 *   calculateBarSizeAndPosition 0x10c3af0 (0xbc bytes, CRC 0x37806af8)
 *
 * Object fields (relative to the SVGScrollBar base):
 *   +0xb0 bar pixel position   +0xb4 bar pixel size
 *   +0xb8 minimum bar size     +0xbc total content units
 *   +0xc0 visible/page units   +0xc4 first visible unit
 *   +0xc8 hide-background byte
 *
 * The track height comes from UIWindow::getWindowRect's bottom-top. The
 * apps-grid scrollbar resource is 1750,180,8x720 (APPS-GRID), so a full-height
 * bar is 720 px; the geometry is independent of the 8 px width.
 */
struct oa_svg_scrollbar_state {
    int32_t bar_pos;
    int32_t bar_size;
    int32_t min_bar;
    int32_t total;
    int32_t visible;
    int32_t first;
    uint8_t background_hidden;
};

struct oa_svg_scrollbar_ops {
    void (*render_background)(struct oa_svg_scrollbar_state *state);
    void (*render_bar)(struct oa_svg_scrollbar_state *state);
};

/* height is the window rect's bottom-top. */
void oa_svg_scrollbar_calculate(struct oa_svg_scrollbar_state *state,
    int32_t height);

/* Returns 1 when the position is accepted and rendered, 0 when the window is
 * not created or first > total. Background rendering is skipped when
 * background_hidden is set; renderBar always runs on the accepted path. */
int oa_svg_scrollbar_set_position(struct oa_svg_scrollbar_state *state,
    int32_t total,int32_t visible,int32_t first,int created,
    const struct oa_svg_scrollbar_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
