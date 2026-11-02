#ifndef OPENAURORA_TEXT_ORIGIN_H
#define OPENAURORA_TEXT_ORIGIN_H
#include <stdint.h>

/* TextRenderer::doVerticalPositioning 1132f64 input fields, in logical units.
 * Numeric justification1=start,2=end,3=center,4=spread. Clip mode2 bypasses
 * the center visible-line-capacity adjustment. No clipping/wrapping is done. */
struct oa_text_vertical {
    int32_t top, bottom, font_height, line_step, line_count;
    unsigned justification, clip_mode;
};
struct oa_text_lines { int32_t first, step; };
/* Pure policy; positive line_count required. Unsupported justification returns
 * ENOTSUP. All failures preserve output; arithmetic overflow is not emulated. */
int oa_text_vertical_position(const struct oa_text_vertical *, struct oa_text_lines *out);
/* setYPositionOfLines1132ed0: floor((ref+logicalY+fontOffset)*n/d)
 * minus floor(ref*n/d), not floor((logicalY+fontOffset)*n/d).
 * Positive denominator, nonnegative numerator/index. No window translation or
 * complete baseline/shaping policy is inferred. All failures preserve output. */
int oa_text_line_y(const struct oa_text_lines *, int32_t index, int32_t reference,
                    int32_t font_offset, int32_t numerator, int32_t denominator, int32_t *out);
#endif
