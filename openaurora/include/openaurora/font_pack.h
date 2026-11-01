#ifndef OPENAURORA_FONT_PACK_H
#define OPENAURORA_FONT_PACK_H
#include <stddef.h>

/* Indexed EF41 theme pack: an index file (record count, then one length per
 * record in text) followed by consecutive binary records. The font ID is the
 * record index. Firmware: DynamicUILoader::loadFontData 0xef5194. */
struct oa_font_pack {
    unsigned count;
    unsigned *lengths;
    unsigned *offsets;
    char *pack_path;
};
int oa_font_pack_init(struct oa_font_pack *pack, const char *index_path,
                      const char *pack_path);
void oa_font_pack_free(struct oa_font_pack *pack);
/* Reads one record; *data is malloc'd and belongs to the caller. */
int oa_font_pack_read(const struct oa_font_pack *pack, unsigned id,
                      unsigned char **data, size_t *size);
#endif
