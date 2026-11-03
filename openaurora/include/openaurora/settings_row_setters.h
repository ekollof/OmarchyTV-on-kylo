#ifndef OPENAURORA_SETTINGS_ROW_SETTERS_H
#define OPENAURORA_SETTINGS_ROW_SETTERS_H
#include <stdint.h>
/* SettingItemsListBoxRow label/progress/range setters. Child choice follows
 * the shipped bodies exactly: label writes row+0x3c4 for type 1 else row+0x104;
 * progress drives row+0x1570 for type 0x17 else row+0x1724 and always formats
 * the stored value into row+0x24e8; range drives row+0x13c0 for type 9 else
 * row+0x1498. Widget updates run only when the child reports created, but the
 * row stores (+0x2dd8 label copy, +0x2dfc value, +0x2df0/+0x2df4 bounds)
 * always run. Percent is oa_setting_row_percent, which keeps the shipped
 * wraparound and __aeabi_idiv divide semantics (see common implementation).
 * Offsets identify firmware-understood child bindings, not pointer arithmetic
 * the consumer should apply to our objects. Callbacks are synchronous, pinned
 * for the call and never retained. NULL row/ops/label, any NULL callback or
 * a short text buffer returns EINVAL before mutation. This does not implement
 * renderItems, UString internals or any widget; text is plain decimal. */
struct oa_setting_row_setter_ops {
    void *context;
    uint32_t (*row_type)(void *row);
    int32_t (*row_min)(void *row);
    int32_t (*row_max)(void *row);
    void *(*child_at)(void *row,uint32_t offset);
    int (*is_created)(void *child);
    void (*set_text)(void *child,const char *text);
    void (*set_percent)(void *bar,int32_t pct);
    void (*set_range)(void *bar,int32_t lo,int32_t hi);
    void (*store_label)(void *row,const char *text);
    void (*store_value)(void *row,int32_t value);
    void (*store_bounds)(void *row,int32_t lo,int32_t hi);
};
int32_t oa_setting_row_percent(int32_t value,int32_t lo,int32_t hi);
int oa_setting_row_set_label(void *row,const struct oa_setting_row_setter_ops *o,const char *label);
int oa_setting_row_set_progress(void *row,const struct oa_setting_row_setter_ops *o,int32_t value);
int oa_setting_row_set_bar_range(void *row,const struct oa_setting_row_setter_ops *o,int32_t lo,int32_t hi);
#endif
