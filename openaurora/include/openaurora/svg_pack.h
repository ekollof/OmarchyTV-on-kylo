#ifndef OPENAURORA_SVG_PACK_H
#define OPENAURORA_SVG_PACK_H
#include "openaurora/binsvg.h"

/* Owned immutable snapshot of index + consecutive BinSVG records. File format
 * and CRC32 name lookup follow DynamicUILoader::loadSVG / DynamicSVGSupplier.
 * Load validates EVERY record before publishing; requires regular files.
 * Project bounds: 65536 records, 4MiB index, 64MiB binary (not firmware limits).
 * Errors are errno values and leave outputs unchanged. No device/render I/O. */
struct oa_svg_pack;
int oa_svg_pack_load(struct oa_svg_pack **out, const char *index, const char *binary);
void oa_svg_pack_free(struct oa_svg_pack *);
unsigned oa_svg_pack_count(const struct oa_svg_pack *);
/* ID is the index position; name is hashed case-sensitively, suffix included,
 * NUL excluded. First matching hash wins, as in firmware. No name table or ELF
 * is needed for name lookup. Returned views/strings borrow the snapshot and
 * expire when it is freed; file edits cannot change published memory. */
int oa_svg_pack_record(const struct oa_svg_pack *, unsigned id, struct oa_binsvg *);
int oa_svg_pack_named(const struct oa_svg_pack *, const char *name, struct oa_binsvg *);
#endif
