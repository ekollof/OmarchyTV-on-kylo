#ifndef OPENAURORA_WIDGET_LAYOUT_H
#define OPENAURORA_WIDGET_LAYOUT_H
#include "openaurora/binsvg.h"

/* First named rectangle after ordered renderGroup + fresh horizontal mask
 * transform. Explicit names must come from the concrete widget/mirror binding.
 * The destination container starts empty: rectangles preceding the first mask
 * are identity, the first mask is stored raw, later rectangles use that cached
 * mask. This is NOT a prelookup of all raw rectangles. Nested groups included.
 * No raster drawing/mirroring/animation is supplied; encountered animation tags
 * return ENOTSUP rather than inventing the renderer's defective seek semantics.
 * Resource must be a validated immutable view. Metadata strings remain borrowed;
 * bounds are transformed. All errors leave output unchanged. */
int oa_widget_group_rect(const struct oa_binsvg *, const char *group,
                         const char *mask, const struct oa_text_rect *surface,
                         const char *name, struct oa_binsvg_rect *out);
/* SVGListBoxRow::renderItems normal/non-mirrored rowItems/mRowM path, followed
 * by SettingItemsListBoxRow's offset by row client top-left. The client is the
 * ACTUAL absolute window/client rectangle, not a raw theme rowPos. Uses local
 * surface(0,0,client.width,client.height); no implicit clipping. Caller chooses
 * the type-dependent child name (text1/labelLong/text2/slidebar/etc). */
int oa_widget_row_item_window(const struct oa_binsvg *, const struct oa_text_rect *client,
                              const char *name, struct oa_text_rect *out);
/* Fresh normal/non-mirrored SVGTextBox, one line, no font/alignment override.
 * Uses full-height row slices (not top/bottom caps), textPos in gRowM, manual
 * float32 height correction and font-marker priority. Returns absolute text
 * surface bounds plus raw metadata and numeric widget alignment/font type.
 * Supported styles must supply mRowM before textPos in gRowM; other layouts
 * fail. A pre-mask textPos needs a separate complete second-pass/cache model.
 * This does not select/load fonts, place glyphs, clip, shape or draw text.
 * Metadata strings borrow the immutable resource; failures preserve output. */
struct oa_widget_textbox {
    struct oa_binsvg_rect text;
    unsigned alignment, font_type;
};
int oa_widget_single_line_textbox(const struct oa_binsvg *, const struct oa_text_rect *window,
                                  struct oa_widget_textbox *out);
#endif
