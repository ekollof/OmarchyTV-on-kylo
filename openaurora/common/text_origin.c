#include "openaurora/text_origin.h"
#include <errno.h>
#include <limits.h>

static int fits(int64_t n) { return n >= INT32_MIN && n <= INT32_MAX; }

int oa_text_vertical_position(const struct oa_text_vertical *p, struct oa_text_lines *out)
{
    if (!p || !out || p->line_count <= 0) return EINVAL;
    if (p->justification < 1 || p->justification > 4) return ENOTSUP;
    int64_t height = (int64_t)p->bottom - p->top, step = p->line_step;
    if (!fits(height)) return EOVERFLOW;
    int64_t count = p->line_count, capacity = step ? (height - p->font_height) / step : 0;
    int64_t block = p->font_height + (count - 1) * step;
    int64_t first = p->top;
    switch (p->justification) {
    case 2: first = p->bottom - block; break;
    case 3:
        if (p->clip_mode != 2 && capacity + 1 < count)
            block = p->font_height + step * capacity;
        first = p->top + (height - block) / 2;
        break;
    case 4:
        if (count > 1 && block < height) step += (height - block) / (count - 1);
        break;
    default: break;
    }
    if (!fits(first) || !fits(step)) return EOVERFLOW;
    *out = (struct oa_text_lines){(int32_t)first,(int32_t)step};
    return 0;
}

static int64_t floor_div(int64_t n, int32_t d)
{
    int64_t q = n / d;
    return q - (n % d < 0);
}

int oa_text_line_y(const struct oa_text_lines *p, int32_t index, int32_t reference,
                    int32_t font_offset, int32_t numerator, int32_t denominator, int32_t *out)
{
    if (!p || !out || index < 0 || numerator < 0 || denominator <= 0) return EINVAL;
    int64_t logical = (int64_t)p->first + (int64_t)index * p->step;
    if (!fits(logical)) return EOVERFLOW;
    int64_t phase = logical + reference + font_offset;
    if (!fits(phase)) return EOVERFLOW;
    int64_t y = floor_div(phase * numerator, denominator) -
                floor_div((int64_t)reference * numerator, denominator);
    if (!fits(y)) return EOVERFLOW;
    *out = (int32_t)y;
    return 0;
}
