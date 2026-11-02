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
#endif
