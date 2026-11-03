#ifndef OPENAURORA_SVG_LIST_WIDGETS_H
#define OPENAURORA_SVG_LIST_WIDGETS_H
#include <stdbool.h>
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
#define OA_SVG_ROW_TEXT_BYTES 64u
/* Temporary text object storage is OA_SVG_ROW_TEXT_BYTES, aligned to
 * max_align_t. Providers must fit that bound, initialize in text_new, consume
 * synchronously in text_set and release in text_free; they must not retain
 * the stack object's address. This is our provider bound, not vendor layout. */
struct oa_svg_row_slide_ops {
    void *context;
    uint32_t (*flag_a)(void *row);
    uint32_t (*flag_b)(void *row);
    uint32_t (*row_type)(void *row);
    void (*set_progress)(void *row,int value);
    void *(*slide_bar)(void *row,uint32_t offset);
    int (*is_created)(void *bar);
    void (*set_slider)(void *bar,int value);
    void (*store_value)(void *row,int value);
    void *(*text_box)(void *row);
    const char *(*text_format)(void *row);
    void (*text_new)(void *buf,const char *format);
    void (*text_append)(void *buf,int value);
    void (*text_set)(void *box,const void *text);
    void (*text_free)(void *buf);
};
/* SettingItemsListBoxRow::setSlideValue. Nonzero +0x2e47 always routes to
 * setProgressValue, as does nonzero +0x2e48 with type23 (+0x2dec). Otherwise
 * the slider at row+0x1498 (or row+0x13c0 when +0x2e48 is clear and the type
 * is 9) is set when created, the value is stored at row+0x2df8, and the text
 * box at row+0x1ca8 shows the value through an empty-format UString when
 * created. The shipped format literal is empty (verified read-only from the
 * dump). All arithmetic wraps; comparisons are exact equality/zero tests.
 * Callbacks are synchronous, pinned for the call and never retained. NULL
 * ops, NULL row or any NULL callback returns EINVAL. Editing targets beyond
 * setProgress/setSlider/setText stay open. */
int oa_setting_row_set_slide(const struct oa_svg_row_slide_ops *o,void *row,int value);
/* SettingItemsListBoxRow::setChecked(bool): query embedded checkbox+0x27a8;
 * only when created, setChecked then store the row's cached byte+0x2e44.
 * An uncreated checkbox does not update the cache. Checked has C bool
 * semantics; raw noncanonical ARM register operands are outside the API.
 * All callbacks receive context first and are pinned/synchronous/nonretaining.
 * Invalid ops or NULL row returns EINVAL before any callback. */
struct oa_svg_row_check_ops {
    void *context;
    void *(*checkbox)(void *,void *row);
    int (*is_created)(void *,void *checkbox);
    void (*set_checked)(void *,void *checkbox,bool checked);
    void (*store_checked)(void *,void *row,bool checked);
};
int oa_setting_row_set_checked(const struct oa_svg_row_check_ops *o,void *row,bool checked);
#endif
