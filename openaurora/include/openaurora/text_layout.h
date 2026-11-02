#ifndef OPENAURORA_TEXT_LAYOUT_H
#define OPENAURORA_TEXT_LAYOUT_H
#include <stdint.h>
/* EASIRect corner order verified through RectHandler::handleSVGElement.
 * Widget-local coordinates, not final scanout coordinates. */
struct oa_text_rect { int32_t left, top, right, bottom; };
/* SVGTextBox::renderBackground 10c624e..10c6296, lines==1 path:
 * rescale textPos height by surface-height/mask-height, keeping top/left/right.
 * Float32 intermediates and truncation toward zero; default FP environment.
 * Reject zero denominator/range errors without changing rect. Does not apply
 * mask translation, row offsets, alignment or a baseline. Returns errno value. */
int oa_text_rect_scale_height(struct oa_text_rect *, int32_t surface, int32_t mask);
/* MaskPositioningTransform eec624/eec424/eec464. A NULL mask represents a
 * failed lookup and leaves rect unchanged. Otherwise subtract mask origin,
 * then apply vendor numeric alignment: 0/1/3/5 left-top, 2/6 right-top,
 * 4/7 left-bottom, 8 right-bottom. Other values apply relative subtraction
 * only. Width/height are preserved. No assumed center alignment or scaling.
 * Bounds errors leave rect unchanged. Surface is required when mask exists. */
int oa_text_rect_position_mask(struct oa_text_rect *rect,
                               const struct oa_text_rect *surface,
                               const struct oa_text_rect *mask, unsigned alignment);
/* SVGListBoxBase::getRowPosition 10bbaa8 runtime fields. Values must come
 * from the actual widget state, not an assumed theme or fitted screenshot. */
struct oa_list_metrics {
    struct oa_text_rect window;
    int32_t vertical_step, row_height; /* +1f4/+1f8 */
    int32_t top, bottom, left, right; /* +1fc/+200/+204/+208 */
    int32_t horizontal_step, item_width; /* +20c/+210 */
    int horizontal, mirrored; /* +e8/+e9, mirror applies only horizontally */
};
/* Negative indices are arithmetic inputs, as in firmware; no item count or
 * scrolling policy is inferred. Mirrored horizontal uses window.right itself,
 * whereas vertical uses window width. Bounds errors leave output unchanged. */
int oa_list_row_rect(const struct oa_list_metrics *, int32_t index,
                     struct oa_text_rect *out);
#endif
