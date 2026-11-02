#ifndef OPENAURORA_BINSVG_H
#define OPENAURORA_BINSVG_H
#include "openaurora/text_layout.h"
#include <stddef.h>
#include <stdint.h>

/* Borrowed immutable ONE BinSVG record, bounded by its pack-index length.
 * No file I/O, drawing, transforms or resource-ID/name guessing. The owner must
 * keep the bytes alive and unchanged through all views/results. Init validates
 * every element/string and balanced groups before publishing the view. */
struct oa_binsvg { const uint8_t *data; size_t size; };
struct oa_binsvg_element {
    unsigned tag; /* 0..11; group end9 has NULL id */
    const char *id; /* empty string is preserved, not a named rectangle */
    size_t offset, size;
};
struct oa_binsvg_rect {
    struct oa_text_rect bounds; /* signed x/y/w/h converted to corners */
    int stroke_width, opacity, begin; /* signed packed fields, no color policy */
    const char *stroke, *fill, *animation, *animate_transform;
};
int oa_binsvg_init(struct oa_binsvg *, const void *data, size_t size);
/* Start offset0; advance only on success. ENOENT at exact end; malformed input
 * leaves offset/result unchanged. Use offsets returned by this iterator. */
int oa_binsvg_next(const struct oa_binsvg *, size_t *offset, struct oa_binsvg_element *);
/* Decode the rectangle at an element offset returned by next(). Includes
 * unnamed drawing rectangles. ENOTSUP for other tags; unchanged on error. */
int oa_binsvg_rectangle(const struct oa_binsvg *, size_t offset, struct oa_binsvg_rect *);
/* First matching named rectangle, like RectangleContainer::getRectangle.
 * NULL group searches the whole record; non-NULL selects the first group of
 * that exact name, including nested groups, and stops at its closing tag.
 * Empty group/name is invalid. ENOENT on missing group/rectangle. All errors
 * leave result unchanged. Raw geometry only: caller applies mirror/transform.
 * Strings remain borrowed, including empty strings. */
int oa_binsvg_find_rect(const struct oa_binsvg *, const char *group,
                        const char *name, struct oa_binsvg_rect *);
#endif
