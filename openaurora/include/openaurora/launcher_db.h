#ifndef OA_LAUNCHER_DB_H
#define OA_LAUNCHER_DB_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/launcher_item.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * settingsDB LauncherItem read adapter.
 *
 * The vendor path is NovoLauncherDataProvider -> AnyLauncherDatabaseAdapter ->
 * SqliteLauncherDatabaseAdapter. `getAllLauncherItems` reads every row;
 * `getLauncherItemsByParentId` keeps rows whose parent matches and, unless the
 * caller asks for them, skips m_is_deleted and m_is_out_of_sync rows. Both
 * decode each row with copyLauncherInfoToItem (0xfac334), implemented here as
 * oa_launcher_info_to_item.
 *
 * The column-to-record mapping comes from
 * NovoLauncherItemInfo::launcher_item_info_struct (0xfabf50): 16 scalar
 * columns and 10 fixed text columns. Text is stored as a fixed-size blob in
 * the database and is truncated to the field size minus one here.
 */
struct oa_launcher_list {
    struct oa_launcher_info *rows;
    size_t count,capacity;
};

/* Zero-initialize the list. The caller must pass a non-empty list only after
 * it came from this adapter; existing rows are released first. */
int oa_launcher_db_read_all(const char *path,struct oa_launcher_list *out,
    char *error,size_t error_size);
int oa_launcher_db_read_parent(const char *path,int32_t parent_id,
    int include_deleted,int include_out_of_sync,struct oa_launcher_list *out,
    char *error,size_t error_size);
void oa_launcher_db_free(struct oa_launcher_list *list);

#ifdef __cplusplus
}
#endif

#endif
