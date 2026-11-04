#ifndef OA_LAUNCHER_ITEM_H
#define OA_LAUNCHER_ITEM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LauncherItem::LauncherItem(NovoLauncherItem const&) at 0xe4794c (Thumb,
 * 500 bytes, CRC 0xd1bcb8c5).
 *
 * This is the settingsDB resource conversion: LauncherDialog::
 * getLauncherItemsList reads NovoLauncherItem rows from the database provider
 * and builds the UI LauncherItem objects with this body. It copies 17 scalar
 * fields, constructs 10 UStrings and one default UString, copies the two-word
 * shared_ptr and increments the control block's reference count, initialises
 * the remaining fields (-1/0), and finally, when the thumbnail-name UString at
 * source +0x3c is non-empty, builds
 * "/buffer/downloaded_images/" + name into item +0x2c. An empty name leaves
 * that slot as the default-constructed empty string.
 *
 * The UString operations stay behind this ops table so the module has no
 * vendor object ABI dependency. Source offsets are read-only; the only write
 * outside the destination item is the shared_ptr control-block refcount at
 * control+4 (the shipped body's atomic branch).
 */
struct oa_launcher_item_ops {
    void (*ustr_copy)(void *dst,const void *src);
    void (*ustr_default)(void *dst);
    int (*ustr_is_empty)(const void *src);
    void (*ustr_assign_literal)(void *dst,const char *literal);
    void (*ustr_append_literal)(void *dst,const char *literal);
    void (*ustr_append)(void *dst,const void *src);
};

/* Destination object extent written by the body: the last store is a word at
 * +0xa0. Source reads reach +0x93 (word at +0x90). */
#define OA_LAUNCHER_ITEM_BYTES 164u
#define OA_NOVO_LAUNCHER_ITEM_READ_BYTES 148u

#define OA_LAUNCHER_IMAGE_DIR "/buffer/downloaded_images"
#define OA_LAUNCHER_IMAGE_SEP "/"

void oa_launcher_item_from_novo(void *item,const void *novo,
    const struct oa_launcher_item_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
