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
    uint8_t background_rendered;
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
 * background_rendered is set; renderBar always runs on the accepted path. */
int oa_svg_scrollbar_set_position(struct oa_svg_scrollbar_state *state,
    int32_t total,int32_t visible,int32_t first,int created,
    const struct oa_svg_scrollbar_ops *ops);

/*
 * SVGScrollBar::calculateMinBarSize (0x10c397c, 0x174 bytes, CRC 0xd9fdda32).
 * The skin looks up the cached scrollbar image at+0x74+widget_state*4 (image
 * id at+0x84+widget_state*4). +6c is SVGWindow's four-state UFA/FA/UFI/FI
 * index, NOT horizontal/vertical orientation. It then resolves widget names
 * 0x20/0x22 and 0x21/0x23 and adds the two moving-bar cap heights. The
 * recovered names are bar_gTopM/bar_mTopM and bar_gBotM/bar_mBotM; the
 * apps-grid caps are 20 and 16 px, so min_bar is 36 for its theme.
 */
struct oa_svg_scrollbar_skin {
    int32_t cached_image[4];
    int32_t image_id[4];
    int32_t min_bar;
    uint32_t widget_state;
};

/*
 * SVGScrollBar::renderBackground (0x10c3658, 0x324 bytes, CRC 0x? ) and
 * renderBar (0x10c3bac, 0x35c bytes). The bodies resize the background/bar
 * SVGSurfaces, configure the top/bottom MaskPositioningTransforms with
 * alignment 3/4, draw the g* images under the m* masks, then stretch the row
 * image with a VerticalMaskScaleTransform. The standard names are
 * gTopM/mTopM, gBotM/mBotM, gRowM/mRowM for the background and
 * bar_gTopM/bar_mTopM, bar_gBotM/bar_mBotM, bar_gRowM/bar_mRowM for the bar.
 * Surface 0 is the background (this+0x98), surface 1 the bar (this+0xa4).
 * +0xc8 is the background-rendered flag (set by renderBackground, cleared by
 * an empty renderBar), not a hide flag.
 */
struct oa_svg_scrollbar_render {
    struct oa_svg_scrollbar_state state;
    struct oa_svg_scrollbar_skin skin;
};
struct oa_svg_scrollbar_render_ops {
    int32_t (*supplier_image)(struct oa_svg_scrollbar_render *r,
        int32_t image_id);
    void (*window_rect)(struct oa_svg_scrollbar_render *r,int32_t rect[4]);
    void (*surface_resize)(struct oa_svg_scrollbar_render *r,int surface,
        const int32_t rect[4]);
    void (*surface_clear)(struct oa_svg_scrollbar_render *r,int surface);
    void (*rect_new)(struct oa_svg_scrollbar_render *r,int32_t rect[4],
        int32_t left,int32_t top,int32_t right,int32_t bottom);
    void (*mask_reset)(struct oa_svg_scrollbar_render *r);
    void (*mask_align)(struct oa_svg_scrollbar_render *r,int align);
    void (*mask_surface_rect)(struct oa_svg_scrollbar_render *r,
        const int32_t rect[4]);
    void (*mask_name)(struct oa_svg_scrollbar_render *r,int32_t widget_id);
    void (*mask_container)(struct oa_svg_scrollbar_render *r);
    void (*renderer_draw)(struct oa_svg_scrollbar_render *r,int surface,
        int32_t image,int32_t widget_id,int transform);
    int32_t (*mask_rect_height)(struct oa_svg_scrollbar_render *r,
        int32_t widget_id);
    void (*vertical_surface_rect)(struct oa_svg_scrollbar_render *r,
        const int32_t rect[4]);
    void (*vertical_name)(struct oa_svg_scrollbar_render *r,int32_t widget_id);
    void (*vertical_container)(struct oa_svg_scrollbar_render *r);
};
void oa_svg_scrollbar_render_background(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *ops);
void oa_svg_scrollbar_render_bar(struct oa_svg_scrollbar_render *r,
    const struct oa_svg_scrollbar_render_ops *ops);
struct oa_svg_scrollbar_skin_ops {
    int32_t (*supplier_image)(struct oa_svg_scrollbar_skin *skin,
        int32_t image_id);
    int32_t (*widget_name)(struct oa_svg_scrollbar_skin *skin,
        int32_t widget_id);
    void (*renderer_background)(struct oa_svg_scrollbar_skin *skin,
        int32_t image,int32_t name);
    int32_t (*rect_height)(struct oa_svg_scrollbar_skin *skin,
        int32_t name);
};
void oa_svg_scrollbar_min_bar(struct oa_svg_scrollbar_skin *skin,
    const struct oa_svg_scrollbar_skin_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
