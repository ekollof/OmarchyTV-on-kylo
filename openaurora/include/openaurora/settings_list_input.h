#ifndef OPENAURORA_SETTINGS_LIST_INPUT_H
#define OPENAURORA_SETTINGS_LIST_INPUT_H
#include <stdint.h>
enum oa_settings_list_route { OA_SL_ROUTE_RIGHT_LEFT,OA_SL_ROUTE_DIGIT,OA_SL_ROUTE_BASE };
struct oa_settings_list_dispatch_ops {
    void *context;
    uint32_t (*run)(void *,enum oa_settings_list_route,const void *);
    const void *message;
};
/* Concrete SettingItemsListBox map; full decoded immutable type/ID. All routes
 * forward the original event and propagate result truth; errno is separate. */
int oa_settings_list_dispatch(const struct oa_settings_list_dispatch_ops *,uint32_t type,uint32_t id,int *handled);
enum oa_settings_list_kind { OA_SL_RIGHT_LEFT,OA_SL_DIGIT };
enum oa_settings_list_query { OA_SL_HORIZONTAL,OA_SL_MIRROR,OA_SL_SELECTED,OA_SL_ROW,OA_SL_RECORD,OA_SL_ROW_VALUE };
enum oa_settings_list_action { OA_SL_ROW_KEY,OA_SL_BASE_KEY,OA_SL_STORE_VALUE };
struct oa_settings_list_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_settings_list_query,uint32_t argument);
    uint32_t (*run)(void *,enum oa_settings_list_action,uint32_t subject,uint32_t value,const void *);
    const void *message;
};
/* Queries are pure reads. ROW/RECORD(selected index) yield nonzero32-bit handles,
 * 0 means missing; handles are not pointers. ROW_VALUE takes the captured row. ROW_KEY carries
 * row/key/original event; BASE_KEY carries0/key/original, or NULL for a mirrored
 * fresh event. STORE_VALUE carries the captured record/value/NULL. The backing
 * record is resolved AFTER row-key selection changes, then original-row value
 * is read and stored. Backend pins both handles/code/context/message through
 * the synchronous call and gates permission/lifecycle/nonblocking work. Nothing
 * is retained. An action proves no UI/hardware success. Digit consumes iff a
 * row exists; Right/Left always consumes, irrespective of lower return values.
 * EINVAL: no callbacks/output write. ENODATA: missing backing record after row
 * delivery, with prior actions retained/handled unchanged. That guard is outside
 * native valid-tree parity, not a claim of a reachable vendor fault. */
int oa_settings_list_input(const struct oa_settings_list_ops *,enum oa_settings_list_kind,uint32_t key,int *handled);
#endif
