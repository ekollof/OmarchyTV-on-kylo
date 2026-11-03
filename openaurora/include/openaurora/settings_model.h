#ifndef OPENAURORA_SETTINGS_MODEL_H
#define OPENAURORA_SETTINGS_MODEL_H
#include "openaurora/settings_draw.h"
struct oa_settings_value_input { uint32_t id;const char *text; };
struct oa_settings_image_input { uint32_t id;const void *data;size_t size; };
struct oa_settings_item_input {
    uint32_t id,type,value,minimum,maximum,min_image,max_image,service_logo,glyph;
    uint8_t active,checked;
    const char *label,*glyph_path,*image_url;
    struct oa_settings_image_input image,additional;
    const struct oa_settings_value_input *values;size_t value_count;
};
struct oa_settings_allocator {
    void *context;
    void *(*allocate)(void *,size_t);
    void (*release)(void *,void *);
};
struct oa_settings_model;
/* Deep-copy a caller-supplied decoded item list. UTF-8 C-string bytes are
 * preserved; no shaping, character indexing or embedded-NUL representation.
 * All string inputs (including empty strings) are required. ok_text is the
 * localized lookupString("OK") result supplied by a resource provider, not
 * an English fallback. Image payloads are opaque owned byte copies; this API
 * neither infers their layout nor dereferences vendor pointers. Non-sentinel
 * images require a nonempty payload. Source storage is needed only during
 * create. Optional allocator must supply both functions and outlive the model.
 * Errors leave *out unchanged and release partial construction. */
int oa_settings_model_create(const struct oa_settings_item_input *,size_t count,const char *ok_text,
                             const struct oa_settings_allocator *,struct oa_settings_model **out);
/* Serialized, synchronous usage; no thread safety is implied. draw holds its
 * own model lifetime: nested draw/destroy returns EBUSY. The row sink receives
 * the original event contract with real owned text/image handles. It must copy
 * anything retained. Type writeback updates the owned model only, not a DB.
 * No input, hardware, renderer, socket, global singleton or locale changes. */
int oa_settings_model_draw(struct oa_settings_model *,int32_t index,void *row,
                           const struct oa_setting_draw_style *,
                           void (*sink)(void *,void *,const struct oa_setting_draw_event *),void *context);
int oa_settings_model_destroy(struct oa_settings_model *);
size_t oa_settings_model_count(const struct oa_settings_model *);
const struct oa_setting_item *oa_settings_model_item(const struct oa_settings_model *,size_t index);
/* Only handles issued by this model/provider are accepted. A borrowed model
 * handle lasts until destroy; event temporary handles last only in the sink. */
const char *oa_settings_text_data(const void *text);
#endif
