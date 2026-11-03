#ifndef OPENAURORA_SETTINGS_ROW_RENDER_H
#define OPENAURORA_SETTINGS_ROW_RENDER_H
#include <stdint.h>
/* SettingItemsListBoxRow::renderItems for row types 0, 1 and 4, plus the
 * immediate 0x1b default and the switch default for any other type outside
 * 2..26. The prologue always runs the base render and three rectangle
 * lookups ("alexa" at ELF 0x14be1b, "text1" at 0x1543bbf, "labelLong" at
 * 0x14df0f4); type 4 additionally looks up "button" at 0x151cfff. With a
 * key2 rectangle every non-4 type runs the shared row+0x104 tail (type 1
 * uses row+0x3c4 instead); only 0x1b exits before it, and a missing key2
 * skips straight to the switch head. The tail's create rejoins the switch:
 * types 0/1/27+ hit the default no-op, while type 4 dispatches case 4
 * (table[2] is 0xd544a4, read from the ELF). Uncreated children get
 * setSVGName rows, created ones get EASIRect math (copy, point query,
 * offset) plus setWindowRect, and every path ends in a create call. All
 * three create sites pass the row label. The row stores are untouched.
 * Widget calls run only on live children, and every store/create follows
 * the shipped order. Offsets identify firmware-understood child bindings,
 * not pointer arithmetic the consumer should apply to our objects. Callbacks
 * are synchronous, pinned for the call and never retained. Type 0x1a (26)
 * runs the shared tail (0x2e8c-in-all-slots branch) then its button case
 * body; the switch bodies for types 2..25 are open and return ENOTSUP
 * (after the shared tail where it runs); NULL row/ops, any NULL callback
 * or a short text buffer returns EINVAL before mutation. This does not
 * implement renderItems for other types, real widgets, UString internals
 * or any drawing. */
struct oa_setting_row_render_ops {
    void *context;
    void (*base_render)(void *row);
    void *(*container_of)(void *row);
    void *(*get_rect)(void *container,const char *key);
    void (*rect_words)(void *rect,uint32_t out[4]);
    uint32_t (*row_type)(void *row);
    uint32_t (*row_word)(void *row,uint32_t offset);
    int (*row_flag)(void *row,uint32_t offset);
    const char *(*row_label)(void *row);
    const char *(*row_button_label)(void *row);
    const char *(*row_ustring)(void *row,uint32_t offset);
    void *(*child_at)(void *row,uint32_t offset);
    int (*is_created)(void *child);
    void (*set_name)(void *child,uint32_t slot,uint32_t id);
    void (*set_focus)(void *child,int on);
    void (*set_elipses)(void *child,int on);
    void (*set_font)(void *child,uint32_t id);
    void (*get_point)(void *row,int32_t pt[2]);
    void (*set_rect)(void *child,const uint32_t rect[4]);
    void (*set_text)(void *child,const char *text);
    void (*create_text)(void *child,uint32_t zero,const char *label,
        const uint32_t rect[4],uint32_t extra,void *parent);
    void (*create_button)(void *button,uint32_t icon,const char *label,
        const uint32_t rect[4],void *parent);
    void (*set_text_alignment)(void *child,uint32_t alignment);
    void (*set_checked)(void *child,uint32_t flag);
    void (*create_checkbox)(void *child,uint32_t zero,uint32_t a,uint32_t b,
        const char *label,const uint32_t rect[4],void *parent);
    void (*set_selected_item)(void *child,uint32_t id);
    void (*create_selection)(void *child,uint32_t zero,uint32_t a,uint32_t b,
        const char *label,const uint32_t rect[4],void *parent);
    void (*destroy_image)(void *child);
    void (*set_entry_mode)(void *child,uint32_t mode);
    void (*set_max_chars)(void *child,uint32_t count);
    void (*set_cursor_auto)(void *child,int on);
    void (*set_numeric_contents)(void *child,uint32_t value);
    void (*create_text_entry)(void *child,uint32_t zero,uint32_t a,uint32_t b,
        const char *label,const uint32_t rect[4],void *parent);
    void (*store_row_word)(void *row,uint32_t offset,uint32_t value);
    void (*destroy_child)(void *child);
    void (*reset_cached_xml)(void *child);
    uint32_t (*glyph_id)(uint32_t op,uint32_t state);
    void (*set_glyph)(void *child,uint32_t id,uint32_t state);
    void (*set_highlight)(void *child,uint32_t id);
    void (*set_sublauncher_highlight)(void *child,uint32_t id);
    void (*set_shadow)(void *child,uint32_t id);
    void (*set_visible)(void *child,int on);
    void (*set_scaling)(void *child,uint32_t type);
    void (*create_image)(void *child,uint32_t id,const uint32_t rect[4],void *parent);
    void (*set_draw_on_top)(void *child,int on);
    void (*create_textbox)(void *child,uint32_t zero,const char *label,
        const uint32_t rect[4],uint32_t extra,void *parent);
    void (*create_slider)(void *child,uint32_t zero,int32_t lo,int32_t hi,
        const uint32_t rect[4],void *parent);
    void (*set_slider_value)(void *child,int32_t value);
    void (*set_slider_focus)(void *child,int on);
    void (*set_slider_range)(void *child,int32_t lo,int32_t hi);
    void (*set_ball_image)(void *child,uint32_t id,uint32_t state);
    void (*create_progressbar)(void *child,uint32_t zero,
        const uint32_t rect[4],void *parent);
    void (*set_percentage)(void *child,uint32_t value);
    void (*set_double_directional)(void *child,int on);
};
int oa_setting_row_render(void *row,const struct oa_setting_row_render_ops *o);
#endif
