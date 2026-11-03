#ifndef OPENAURORA_SVG_LIST_WIDGETS_H
#define OPENAURORA_SVG_LIST_WIDGETS_H
#include <stdint.h>
/* Widget-level row/list getters behind the typed row-value router.
 * SettingItemsListBoxRow::getNumericContents forwards to the embedded
 * SVGTextEntryBox at row+0x18d8; the entry handle is passed through
 * untouched. SettingItemsListBox::getSelectedItemId returns 0xffffffff when
 * the list is not created or the selected item is -1, else the first word of
 * the entry at index selectedItem in the array behind list+0x28c. All
 * arithmetic wraps uint32_t; the (item+1)==0 none-test matches the shipped
 * adds/beq. Callbacks are synchronous, pinned for the call and never
 * retained. NULL ops or any NULL callback returns EINVAL with the error
 * out-param set and a zero/0xffffffff result respectively. Getters return raw
 * words; row editing, parent refresh and render targets stay open. */
struct oa_svg_row_text_ops {
    void *context;
    void *(*text_entry)(void *row);
    uint32_t (*get_numeric)(void *entry);
};
uint32_t oa_setting_row_numeric(const struct oa_svg_row_text_ops *o,int *error);
struct oa_svg_list_select_ops {
    void *context;
    int (*is_created)(void *list);
    uint32_t (*selected_item)(void *list);
    void *(*id_table)(void *list);
    void *(*id_entry)(void *table,uint32_t index);
    uint32_t (*entry_id)(void *entry);
};
uint32_t oa_setting_list_selected_id(const struct oa_svg_list_select_ops *o,int *error);
#endif
