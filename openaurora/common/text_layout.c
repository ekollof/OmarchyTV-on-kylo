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

static int64_t rect_width(const struct oa_text_rect *r)
{ return r ? (int64_t)r->right - r->left : 0; }
static int64_t rect_height(const struct oa_text_rect *r)
{ return r ? (int64_t)r->bottom - r->top : 0; }
static int fits32(int64_t n) { return n >= INT32_MIN && n <= INT32_MAX; }

int oa_list_measure_slices(struct oa_list_metrics *m, const struct oa_list_slices *s, int title)
{
    if (!m || !s) return EINVAL;
    int64_t step = rect_height(s->row_left), horizontal = rect_width(s->row_left);
    int64_t top = rect_height(s->top_left), bottom = rect_height(s->bottom_left);
    if (m->horizontal) horizontal += rect_width(s->row_middle) + rect_width(s->row_right);
    if (title) top += step;
    if (!fits32(step) || !fits32(horizontal) || !fits32(top) || !fits32(bottom)) return EOVERFLOW;
    m->vertical_step = (int32_t)step; m->horizontal_step = (int32_t)horizontal;
    m->top = (int32_t)top; m->bottom = (int32_t)bottom;
    return 0;
}

int oa_list_measure_painted_row(struct oa_list_metrics *m, const struct oa_text_rect *row,
                                const struct oa_text_rect *middle,
                                const struct oa_text_rect *left_mask,
                                const struct oa_text_rect *right_mask)
{
    if (!m) return EINVAL;
    if (!row) return 0;
    if (!middle) return EINVAL;
    int64_t height = rect_height(row), width = rect_width(row);
    int64_t left = rect_width(left_mask) + row->left - (int64_t)middle->left;
    int64_t right = rect_width(right_mask) + middle->right - (int64_t)row->right;
    if (!fits32(height) || !fits32(width) || !fits32(left) || !fits32(right)) return EOVERFLOW;
    m->row_height = (int32_t)height; m->item_width = (int32_t)width;
    m->left = (int32_t)left; m->right = (int32_t)right;
    return 0;
}

int oa_list_update_visible_count(const struct oa_list_metrics *m, int32_t *count)
{
    if (!m || !count) return EINVAL;
    int32_t step = m->horizontal ? m->horizontal_step : m->vertical_step;
    if (step <= 0) return 0;
    int64_t span = m->horizontal ? rect_width(&m->window) - m->left - m->right :
                                  rect_height(&m->window) - m->top - m->bottom;
    /* Reject overflowing firmware subtraction rather than wrapping it. */
    if (!fits32(span)) return EOVERFLOW;
    *count = (int32_t)(span / step);
    return 0;
}

int oa_horizontal_scale_measure(struct oa_horizontal_scale *s, const struct oa_text_rect *surface,
                                 const struct oa_text_rect *mask)
{
    if (!s) return EINVAL;
    if (!mask) return 0;
    if (!surface) return EINVAL;
    int64_t mw = rect_width(mask), sw = rect_width(surface);
    if (!fits32(mw) || !fits32(sw)) return EOVERFLOW;
    if (mw > 0) s->ratio = sw <= 0 ? 0.0 : (double)sw / (double)mw;
    return 0;
}

static int scale_span(int64_t span, double ratio, int32_t *out)
{
    if (!fits32(span)) return EOVERFLOW;
    double scaled = (double)span * ratio;
    if (!isfinite(scaled) || scaled < INT32_MIN || scaled >= 2147483648.0) return EOVERFLOW;
    *out = (int32_t)scaled;
    return 0;
}

int oa_text_rect_stretch_mask(struct oa_text_rect *r, const struct oa_text_rect *surface,
                              const struct oa_text_rect *mask, const struct oa_horizontal_scale *s)
{
    if (!r) return EINVAL;
    if (!mask) return 0;
    if (!surface || !s || !isfinite(s->ratio)) return EINVAL;
    int32_t offset, width;
    int rc = scale_span((int64_t)r->left - mask->left, s->ratio, &offset);
    if (rc) return rc;
    rc = scale_span(rect_width(r), s->ratio, &width);
    if (rc) return rc;
    int64_t left = (int64_t)surface->left + offset, right = left + width;
    int64_t top = (int64_t)r->top - mask->top + surface->top;
    int64_t bottom = (int64_t)r->bottom - mask->top + surface->top;
    if (!fits32(left) || !fits32(right) || !fits32(top) || !fits32(bottom)) return EOVERFLOW;
    *r = (struct oa_text_rect){(int32_t)left,(int32_t)top,(int32_t)right,(int32_t)bottom};
    return 0;
}

int oa_horizontal_middle_surface(const struct oa_text_rect *outer,
                                  const struct oa_text_rect *left_mask,
                                  const struct oa_text_rect *right_mask,
                                  struct oa_text_rect *out)
{
    if (!outer || !out) return EINVAL;
    int64_t left = outer->left + rect_width(left_mask);
    int64_t right = outer->right - rect_width(right_mask);
    if (!fits32(left) || !fits32(right)) return EOVERFLOW;
    *out = (struct oa_text_rect){(int32_t)left,outer->top,(int32_t)right,outer->bottom};
    return 0;
}
