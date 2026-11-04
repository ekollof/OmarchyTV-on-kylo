#ifndef OA_SETTING_ROW_CONSTRUCT_H
#define OA_SETTING_ROW_CONSTRUCT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SettingItemsListBoxRow::SettingItemsListBoxRow(UIFramework&, SVGContext&)
 * (0xd53d0c, Thumb), child-construction and default-field path.
 *
 * Recovers the ordered child constructors and the inline default field
 * stores. The constructors themselves are vendor calls behind this ops table.
 */
struct oa_setting_row_construct_ops {
    void (*construct_base)(void *row,void *framework,void *context);
    void (*construct_textbox)(void *child,void *framework,void *context);
    void (*construct_button)(void *child,void *framework,void *context);
    void (*construct_selection_box)(void *child,void *framework,void *context);
    void (*construct_slide_bar)(void *child,void *framework,void *context);
    void (*construct_progress_bar)(void *child,void *framework,void *context);
    void (*construct_text_entry_box)(void *child,void *framework,void *context);
    void (*construct_check_box)(void *child,void *framework,void *context);
    void (*construct_image)(void *child,void *framework,void *context);
    /* literal==NULL selects UString's default constructor. */
    void (*construct_ustring)(void *slot,const char *literal);
    void *(*control_interface)(void *framework);
    int (*horizontal_mirror)(void *control);
};

void oa_setting_row_construct(void *row,void *framework,void *context,
    const struct oa_setting_row_construct_ops *o);

/* Offsets of the default fields written by the constructor. */
enum {
    OA_ROW_FIELD_COUNT = 47
};
extern const uint32_t oa_setting_row_field_offsets[OA_ROW_FIELD_COUNT];

#ifdef __cplusplus
}
#endif

#endif
