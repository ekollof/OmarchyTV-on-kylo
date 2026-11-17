#ifndef OPENAURORA_APPS_LABEL_H
#define OPENAURORA_APPS_LABEL_H

#include "openaurora/font_bitmap.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Render one apps-grid label with the recovered single-line centering rule
 * (UI-HORIZONTAL-ORIGIN-2026-11-02): SVGTextBox/LineObj centers
 *   floor((available - physicalWidth)/2) + firstAdjustment
 * where physicalWidth is the glyph-advance sum with the first/last bearing
 * adjustments, NOT the ink bounding box. Centering the ink box shifts labels
 * with an asymmetric left bearing (the recorded 2 px Twitter/YouTube Music
 * residual in STOCK-APPS-GRID-PARITY).
 *
 * The helper draws into a private cell-sized canvas and returns an owned RGBA
 * crop of the ink plus its x offset inside the cell. It performs no display or
 * host I/O. `cell_w` is the available width (the full grid cell); `margin`
 * keeps the paper style used by the existing caller. Returns 0, or a negative
 * errno; `*rgba` is NULL on failure. The caller frees the crop.
 */
int oa_apps_label_render(const struct oa_bitmap_font *font, const char *text,
                         int cell_w, int margin, unsigned char **rgba,
                         int *x, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif
