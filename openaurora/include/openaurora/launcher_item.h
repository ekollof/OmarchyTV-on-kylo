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

/*
 * NovoLauncherItemInfo — the fixed record the settingsDB adapter fills from
 * one LauncherItem row. Offsets and text sizes are recovered from
 * NovoLauncherItemInfo::launcher_item_info_struct (0xfabf50), which builds the
 * member map used by SQLiteDB::SequentialQuery: 16 scalar columns and 10 text
 * columns. The DB blobs are stored at exactly these sizes (300/60/150 bytes).
 */
struct oa_launcher_info {
    uint32_t reserved;                 /* +0x000 */
    uint32_t image_crc;                /* +0x004 m_application_image_crc */
    uint32_t thumb_update_time;        /* +0x008 m_application_thumb_update_time */
    uint32_t application_update_time;  /* +0x00c m_application_update_time */
    int32_t channel_handle;            /* +0x010 m_channel_handle */
    int32_t id;                        /* +0x014 m_id */
    uint8_t is_delete_disabled;        /* +0x018 m_is_delete_disabled */
    uint8_t is_deleted;                /* +0x019 m_is_deleted */
    uint8_t is_featured;               /* +0x01a m_is_featured */
    uint8_t is_moved_by_user;          /* +0x01b m_is_moved_by_user */
    uint8_t is_out_of_sync;            /* +0x01c m_is_out_of_sync */
    uint8_t reserved_flags[3];         /* +0x01d */
    int32_t item_type;                 /* +0x020 m_item_type */
    int32_t menu_return_id;            /* +0x024 m_menu_return_id */
    int32_t order_index;               /* +0x028 m_order_index */
    int32_t original_id;               /* +0x02c m_original_id */
    int32_t parent_id;                 /* +0x030 m_parent_id */
    int32_t server_id;                 /* +0x034 m_server_id */
    char ad_image_url[300];            /* +0x038 */
    char ad_tracking_url[300];         /* +0x164 */
    char ad_url[300];                  /* +0x290 */
    char application_name[60];         /* +0x3bc */
    char thumb_file_name[300];         /* +0x3f8 */
    char thumb_url[300];               /* +0x524 */
    char application_url[300];         /* +0x650 */
    char fvp_application_id[300];      /* +0x77c */
    char menu_string_id[150];          /* +0x8a8 */
    char recommendation_url[300];      /* +0x93e */
};

/*
 * SqliteLauncherDatabaseAdapter::copyLauncherInfoToItem (0xfac334): converts
 * one decoded record into the NovoLauncherItem intermediate. Text fields are
 * assigned with UString::operator=(char const*) through the same ops table.
 */
void oa_launcher_info_to_item(void *item,const struct oa_launcher_info *info,
    const struct oa_launcher_item_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
