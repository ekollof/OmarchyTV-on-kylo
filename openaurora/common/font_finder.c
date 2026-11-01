#include "openaurora/font_finder.h"
#include <errno.h>
#include <limits.h>

unsigned oa_font_type_from_markers(unsigned mask)
{
    for (unsigned type = OA_FONT_REGULAR; type <= OA_FONT_LIGHT_ITALIC; ++type)
        if (mask & (1u << type)) return type;
    return OA_FONT_REGULAR;
}

/* UIFrameworkFontFinder::addUIFrameworkFont eef0c0, stable sorts eeef74/
 * eeefe4, getFont eeedc0, findMatchFont eeed60. */
int oa_font_finder_add(struct oa_font_finder *f, unsigned id, int height, unsigned type)
{
    if (!f || f->count > OA_FONT_FINDER_CAPACITY) return EINVAL;
    if (f->count == OA_FONT_FINDER_CAPACITY) return ENOSPC;
    if (type > 5) type = 0;
    size_t at = f->count;
    while (at && f->entries[at - 1].height > height) {
        f->entries[at] = f->entries[at - 1];
        --at;
    }
    f->entries[at] = (struct oa_font_entry){ id, height, type };
    ++f->count;
    return 0;
}

int oa_font_finder_select(const struct oa_font_finder *f, int height, int lines,
                           unsigned type, unsigned *id)
{
    if (!f || !id || f->count > OA_FONT_FINDER_CAPACITY) return EINVAL;
    if (!f->count) { *id = 0; return 0; }
    if (!lines || (height == INT_MIN && lines == -1)) return EDOM;
    int available = height / lines;
    if (type > 5) type = 0;
    int exists = 0;
    for (size_t i = 0; i < f->count; ++i)
        if (f->entries[i].type == type) exists = 1;
    if (!exists) type = 0;
    unsigned selected = f->entries[0].id;
    for (size_t i = 0; i < f->count; ++i) {
        const struct oa_font_entry *e = &f->entries[i];
        if (e->type != type) continue;
        if (e->height <= 0 || e->height > available) break;
        selected = e->id;
    }
    *id = selected;
    return 0;
}
