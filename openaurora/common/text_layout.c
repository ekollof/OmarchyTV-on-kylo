#include "openaurora/text_layout.h"
#include <errno.h>
#include <limits.h>
#include <math.h>

int oa_text_rect_scale_height(struct oa_text_rect *r, int32_t surface, int32_t mask)
{
    if (!r) return EINVAL;
    if (!mask) return EDOM;
    int64_t span = (int64_t)r->bottom - r->top;
    if (span < INT32_MIN || span > INT32_MAX) return EOVERFLOW;
    /* Preserve division then multiplication, not an algebraic reassociation. */
    float ratio = (float)surface / (float)mask;
    float scaled = ratio * (float)span;
    if (!isfinite(scaled) || scaled >= 2147483648.0f || scaled < -2147483648.0f)
        return EOVERFLOW;
    int64_t bottom = (int64_t)r->top + (int32_t)scaled;
    if (bottom < INT32_MIN || bottom > INT32_MAX) return EOVERFLOW;
    r->bottom = (int32_t)bottom;
    return 0;
}
