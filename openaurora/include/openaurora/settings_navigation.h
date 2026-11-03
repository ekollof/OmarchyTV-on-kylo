#ifndef OPENAURORA_SETTINGS_NAVIGATION_H
#define OPENAURORA_SETTINGS_NAVIGATION_H
#include <stdint.h>
enum oa_settings_nav_query {
    OA_SN_COUNT,OA_SN_CURSOR,OA_SN_ACTIVE_ID,OA_SN_ITEM_INDEX,OA_SN_BIGGEST_VISIBLE,OA_SN_ROWS
};
enum oa_settings_nav_action { OA_SN_SET_CURSOR,OA_SN_SET_ITEM,OA_SN_SET_ROW };
struct oa_settings_nav_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_settings_nav_query,uint32_t argument);
    void (*run)(void *,enum oa_settings_nav_action,uint32_t argument);
};
/* Qualified SettingsDialog::navigateInList. COUNT is a valid ARM vector length
 * (<=0x1fffffff); CURSOR indexes its active item IDs, not widget rows. ACTIVE_ID
 * takes cursor, ITEM_INDEX takes ID. Widget index/biggest/rows preserve raw32
 * bits; comparisons are signed32, arithmetic wraps as ARM instructions do.
 * SET_CURSOR is a direct state write and must only store its argument; subsequent
 * item/row callbacks may change state. Captured geometry is not re-read after
 * SET_ITEM. Caller pins immutable active-ID storage/context/code through this
 * synchronous call, with permission/lifetime gates and nonblocking actions.
 * No pointer/event is retained; this method does not forward or allocate keys.
 * Return0 on success (including empty vector). EINVAL: no callbacks. EOVERFLOW:
 * count outside valid-vector domain. ERANGE: resulting cursor cannot index the
 * vector, possibly after its state write; this prevents a native invalid read,
 * not parity acceptance for invalid trees. No widget action follows that error. */
int oa_settings_navigate(const struct oa_settings_nav_ops *,uint32_t ice_key);
#endif
