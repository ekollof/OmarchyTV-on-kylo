#ifndef OPENAURORA_FONT_FINDER_H
#define OPENAURORA_FONT_FINDER_H
#include <stddef.h>

#define OA_FONT_FINDER_CAPACITY 42u
struct oa_font_entry { unsigned id; int height; unsigned type; };
/* Zero-initialize before adding. Entries remain stably height-sorted. */
struct oa_font_finder {
    struct oa_font_entry entries[OA_FONT_FINDER_CAPACITY];
    size_t count;
};
int oa_font_finder_add(struct oa_font_finder *, unsigned id, int height, unsigned type);
/* SVGTextBox passes text rectangle height / line count (signed division).
 * Empty/unknown type uses type0. No fitting font uses the global smallest.
 * Zero lines and INT_MIN/-1 return an error instead of vendor SIGFPE/overflow.
 * Output is unchanged on error. No fonts returns id0 as vendor does. */
int oa_font_finder_select(const struct oa_font_finder *, int height, int lines,
                           unsigned type, unsigned *id);
#endif
