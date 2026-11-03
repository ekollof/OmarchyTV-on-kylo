#ifndef OPENAURORA_SVG_LIST_ANIM_H
#define OPENAURORA_SVG_LIST_ANIM_H
#include <stdint.h>
/* startSelectAnimation body, not a scheduler/rendering engine.
 * Every callback receives context. std_name/new/sub each return a distinct
 * owned opaque UString handle (NULL = allocation failure), released by free.
 * Name is a nullable borrowed C string (firmware constructs empty on NULL);
 * duration text is borrowed. Length/sub use firmware character
 * units, not byte-pointer arithmetic; providers must bound empty substrings
 * and the source's wrapping length-1. Equality operands are substring,standard.
 * Parsing precedes temporary destruction; convert to float before multiplying.
 * Container count is signed32 in firmware: negative raw counts skip the scan.
 * Caller pins code/objects/context and gates nonblocking work. Nothing retained.
 * EINVAL executes nothing. ENOMEM/ENODATA may follow rectangle writes but release
 * every acquired string. These guards are outside native invalid-tree parity.
 * started changes only after a start callback returns; no action proves motion. */
struct oa_svg_list_anim_ops {
    void *context;
    int (*anim_enabled)(void *,void *list);
    uint32_t (*selected_item)(void *,void *manager);
    void *(*item_row)(void *,void *manager,uint32_t index);
    void (*get_rect)(void *,void *row,uint32_t rect[4]);
    void (*read_cur)(void *,void *list,uint32_t out[4]);
    void (*write_prev)(void *,void *list,const uint32_t in[4]);
    void (*write_cur)(void *,void *list,const uint32_t in[4]);
    void *(*std_name)(void *,void *list,uint32_t id);
    uint32_t (*anim_count)(void *,void *container);
    void *(*anim_at)(void *,void *container,uint32_t index);
    const char *(*anim_name)(void *,void *anim);
    const char *(*anim_text)(void *,void *anim);
    void *(*anim_controller)(void *,void *owner);
    void *(*anim_owner)(void *,void *list);
    void *(*anim_container)(void *,void *list);
    void *(*anim_target)(void *,void *list);
    void (*anim_start)(void *,void *controller,void *target,float value);
    void *(*ustr_new)(void *,const char *borrowed);
    uint32_t (*ustr_len)(void *,const void *owned);
    void *(*ustr_sub)(void *,const void *owned,uint32_t start,uint32_t length);
    int (*ustr_equal)(void *,const void *left,const void *right);
    void (*ustr_free)(void *,void *owned);
    double (*parse_duration)(void *,const char *borrowed);
};
int oa_svg_list_start_animation(const struct oa_svg_list_anim_ops *,void *list,void *manager,float *started);
#endif
