#ifndef OPENAURORA_SETTINGS_ROW_TYPE_H
#define OPENAURORA_SETTINGS_ROW_TYPE_H
#include <stdint.h>
struct oa_setting_row_type_state { uint32_t type;uint8_t rebuild; };
enum oa_setting_child_kind { OA_SETTING_WINDOW,OA_SETTING_SELECTION,OA_SETTING_TEXT_ENTRY,OA_SETTING_CHECKBOX };
struct oa_setting_row_type_ops {
    void *context;
    void (*render_items)(void *);
    int (*is_visible)(void *);
    void (*set_visible)(void *,enum oa_setting_child_kind,uint32_t child_offset,int visible);
};
/* SettingItemsListBoxRow::setType. Cache+0x2dec and rebuild byte+0x2e49 are
 * stored before renderItems virtual+0x5c, which runs only on a type change.
 * Unchanged type clears rebuild. Visibility is queried AFTER rendering, then
 * qualified child setters run in exact firmware order. Unknown types change
 * only the two label children when visible. Type24 does not touch child+1ca8.
 * Offsets identify firmware-understood child bindings, not pointer arithmetic
 * the consumer should apply to our objects. Callbacks are synchronous/pinned,
 * retain nothing and must not mutate type/rebuild; render may change visibility.
 * All callbacks are required; invalid ops/state return EINVAL before mutation.
 * This does not implement renderItems or the child widget visibility methods. */
int oa_setting_row_set_type(struct oa_setting_row_type_state *,const struct oa_setting_row_type_ops *,uint32_t type);
#endif
