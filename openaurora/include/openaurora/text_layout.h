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
/* calculateHeight 10bb7ec: raw named slices, before rendering transforms.
 * Missing slices count as zero. Only top/bottom/steps are overwritten; the
 * painted row size and side insets remain cached. title adds one vertical step.
 * Input names are resolved before calling; this API does not parse a theme. */
struct oa_list_slices {
    const struct oa_text_rect *top_left, *row_left, *row_middle, *row_right, *bottom_left;
};
int oa_list_measure_slices(struct oa_list_metrics *, const struct oa_list_slices *, int title);
/* renderBackground 10bda60..10bda9a: apply rowPos after each painted
 * middle slice. NULL rowPos preserves cached values. Missing side masks have
 * zero width. middle is that slice's destination surface (required for rowPos).
 * This must not be applied to raw resource coordinates. */
int oa_list_measure_painted_row(struct oa_list_metrics *, const struct oa_text_rect *row_pos,
                                const struct oa_text_rect *middle,
                                const struct oa_text_rect *left_mask,
                                const struct oa_text_rect *right_mask);
/* Count is updated before painted-row insets in firmware. Nonpositive step
 * preserves the previous count. Do not recompute mid-paint from new insets. */
int oa_list_update_visible_count(const struct oa_list_metrics *, int32_t *count);
/* HorizontalMaskScaleTransform's retained double ratio. Resource lookup/cache
 * validity is external: changing the surface alone does not remeasure in the
 * firmware. Call measure only when its getMask would resolve a fresh mask. */
struct oa_horizontal_scale { double ratio; };
#define OA_HORIZONTAL_SCALE_INITIALIZER {1.0}
/* Missing/nonpositive-width mask retains ratio. Positive mask width with
 * nonpositive surface width sets0; otherwise double surfaceWidth/maskWidth. */
int oa_horizontal_scale_measure(struct oa_horizontal_scale *, const struct oa_text_rect *surface,
                                 const struct oa_text_rect *mask);
/* Missing mask is identity. Otherwise scale relative X and width separately
 * (double, truncation), translate Y without scaling, then reconstruct corners.
 * Invalid/range failures leave output unchanged. Default FP environment. */
int oa_text_rect_stretch_mask(struct oa_text_rect *, const struct oa_text_rect *surface,
                              const struct oa_text_rect *mask, const struct oa_horizontal_scale *);
/* List/row renderBackground constructs the horizontal middle destination by
 * removing side-mask widths from an outer surface; top/bottom are unchanged.
 * NULL side mask means zero width. Bounds errors preserve output. */
int oa_horizontal_middle_surface(const struct oa_text_rect *outer,
                                  const struct oa_text_rect *left_mask,
                                  const struct oa_text_rect *right_mask,
                                  struct oa_text_rect *out);
#endif
