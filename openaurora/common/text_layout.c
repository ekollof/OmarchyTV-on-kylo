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

int oa_text_rect_position_mask(struct oa_text_rect *r,
                               const struct oa_text_rect *surface,
                               const struct oa_text_rect *mask, unsigned alignment)
{
    if (!r) return EINVAL;
    if (!mask) return 0;
    if (!surface) return EINVAL;
    int64_t dx = -(int64_t)mask->left, dy = -(int64_t)mask->top;
    if (alignment <= 8) {
        int right = alignment == 2 || alignment == 6 || alignment == 8;
        int bottom = alignment == 4 || alignment == 7 || alignment == 8;
        dx += right ? (int64_t)surface->right - ((int64_t)mask->right - mask->left) : surface->left;
        dy += bottom ? (int64_t)surface->bottom - ((int64_t)mask->bottom - mask->top) : surface->top;
    }
    int64_t left = r->left + dx, top = r->top + dy;
    int64_t right = r->right + dx, bottom = r->bottom + dy;
    if (left < INT32_MIN || left > INT32_MAX || top < INT32_MIN || top > INT32_MAX ||
        right < INT32_MIN || right > INT32_MAX || bottom < INT32_MIN || bottom > INT32_MAX)
        return EOVERFLOW;
    *r = (struct oa_text_rect){(int32_t)left,(int32_t)top,(int32_t)right,(int32_t)bottom};
    return 0;
}

int oa_list_row_rect(const struct oa_list_metrics *m, int32_t index,
                     struct oa_text_rect *out)
{
    if (!m || !out) return EINVAL;
    int64_t left, top, right, bottom;
    if (!m->horizontal) {
        left = m->left;
        top = (int64_t)m->vertical_step * index + m->top;
        right = (int64_t)m->window.right - m->window.left - m->right;
        bottom = top + m->row_height;
    } else {
        left = m->mirrored ? (int64_t)m->window.right - m->left -
                              ((int64_t)index + 1) * m->horizontal_step :
                              (int64_t)m->left + (int64_t)index * m->horizontal_step;
        top = m->top;
        right = left + m->item_width;
        bottom = (int64_t)m->window.bottom - m->window.top - m->bottom;
    }
    if (left < INT32_MIN || left > INT32_MAX || top < INT32_MIN || top > INT32_MAX ||
        right < INT32_MIN || right > INT32_MAX || bottom < INT32_MIN || bottom > INT32_MAX)
        return EOVERFLOW;
    *out = (struct oa_text_rect){(int32_t)left,(int32_t)top,(int32_t)right,(int32_t)bottom};
    return 0;
}
