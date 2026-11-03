#ifndef OPENAURORA_SETTINGS_DRAW_H
#define OPENAURORA_SETTINGS_DRAW_H
#include <stddef.h>
#include <stdint.h>
struct oa_setting_value { uint32_t id;const void *text; };
struct oa_setting_values { const struct oa_setting_value *items;size_t count;void *owner; };
struct oa_setting_image { uint32_t id;const void *handle; };
struct oa_setting_item {
    uint32_t id,type,value,minimum,maximum,min_image,max_image,service_logo,glyph;
    uint8_t active,checked;
    const void *label,*glyph_path,*image_url;
    struct oa_setting_image image,additional;
    const struct oa_setting_value *values;
    size_t value_count;
};
struct oa_setting_draw_style { uint8_t no_default,ball,progress,mixed; };
enum oa_setting_draw_action {
    OA_SD_ROW_ID,OA_SD_PROGRESS_MODE,OA_SD_MIXED_MODE,OA_SD_BALL,OA_SD_LONG_BUTTON,
    OA_SD_BAR_IMAGES,OA_SD_TYPE,OA_SD_ACTIVE,OA_SD_BUTTON_TEXT,OA_SD_IMAGE_VISIBLE,
    OA_SD_SELECTABLE,OA_SD_ADDITIONAL,OA_SD_GLYPH_PATH,OA_SD_SERVICE_LOGO,
    OA_SD_IMAGE_DETAIL,OA_SD_IMAGE_URL,OA_SD_GLYPH,OA_SD_LABEL,OA_SD_CLEAR_ITEMS,
    OA_SD_APPEND_ITEM,OA_SD_SELECTED_ID,OA_SD_SELECTION_IMAGE,OA_SD_RANGE,
    OA_SD_SLIDE,OA_SD_PROGRESS,OA_SD_MINMAX_TEXT,OA_SD_NUMERIC,
    OA_SD_TEXT,OA_SD_DATE,OA_SD_TIME,OA_SD_DURATION,OA_SD_PASSWORD,OA_SD_IP,OA_SD_CHECKED
};
struct oa_setting_draw_event {
    enum oa_setting_draw_action action;
    uint32_t a,b;
    const void *text,*text2,*image;
};
struct oa_setting_draw_ops {
    void *context;
    void (*run)(void *,void *row,const struct oa_setting_draw_event *);
    void (*store_type)(void *,struct oa_setting_item *,uint32_t);
    /* Successful copy owns a distinct immutable value snapshot, even empty.
     * Failure returns errno with no owner to drop. Valid copies have count
     * entries, borrowed texts valid until values_drop. */
    int (*values_copy)(void *,const struct oa_setting_item *,struct oa_setting_values *);
    void (*values_drop)(void *,struct oa_setting_values *);
    /* NULL source constructs empty. Every successful new returns a distinct
     * owned object; NULL signals ENOMEM. Other text callbacks are infallible.
     * Borrowed item/value handles are never destroyed by this controller. */
    void *(*text_new)(void *,const void *source);
    void (*text_assign)(void *,void *target,const void *source);
    void (*text_append_int)(void *,void *,uint32_t raw_signed32);
    int (*text_empty)(void *,const void *);
    int (*text_equals_empty)(void *,const void *);
    void (*text_free)(void *,void *);
    void (*lookup_default)(void *,void *text); /* resourceManager -> lookupString("OK",text) */
};
/* SettingItemsListBox::drawRow. Pinned decoded valid-tree inputs; item/value
 * arrays, non-NULL text handles, image handles and callbacks live through the
 * synchronous call. NULL passed internally to text_new means an empty string;
 * it is not a missing borrowed source. values_copy may return an empty snapshot.
 * Fields are pure reads; actions cannot mutate the input model except that
 * store_type MUST update item->type and its backing provider. No event or
 * temporary handle may be retained. Events forward raw word/byte operands;
 * a/b meaning follows the named setter; text/text2/image are opaque handles.
 * Bounds use signed count/index. Progress mode writes types9..11 ->23..25;
 * mixed mode writes only9->23. Copies/free and setters preserve firmware order.
 * EINVAL before callbacks for invalid ops/row/style; ENODATA for missing item;
 * resource failures unwind owned temporaries, retain prior actions/writeback
 * and return errno. These guards are outside firmware valid-tree parity.
 * This dispatch does not implement lower widgets, text/vector providers,
 * resource lookup or display ownership/rendering. */
int oa_settings_draw_row(const struct oa_setting_draw_ops *,struct oa_setting_item *const *items,
                         int32_t count,int32_t index,void *row,const struct oa_setting_draw_style *);
#endif
