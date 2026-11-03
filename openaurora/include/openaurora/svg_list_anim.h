#ifndef OPENAURORA_SVG_LIST_ANIM_H
#define OPENAURORA_SVG_LIST_ANIM_H
#include <stdint.h>
/* SVGListBoxBase::startSelectAnimation. Runs when the selected row changes
 * and after every handled key/move/update. Returns silently when animation is
 * disabled, when there is no selected row, or when the row rectangle matches
 * the stored +0x25c words. Otherwise it archives the stored rectangle to
 * +0x24c, stores the fresh rectangle to +0x25c, and starts the first listed
 * animation whose name (minus its first character) equals the widget's
 * standard name, with value atof(duration)*1000.0f. The 1000.0f scale is read
 * from the shipped literal pool (ELF file offset of 0x10bc04c); the +0x98
 * target, +0x280 container and +0x4 owner words and the +12 name/+28 duration
 * animation fields are list layout facts supplied by callbacks. UString
 * construction/length/substring/compare/destroy run as owned plain-C-string
 * calls in the exact shipped order (ASCII test scope: length is byte count,
 * substring is pointer+start); the shipped (start1,length-1) arguments are
 * asserted. Native atof and the C library atof observe the same input
 * string. Callbacks are synchronous, pinned for the call and never retained.
 * NULL ops or any NULL callback returns EINVAL. The animation controller,
 * theme names and visible motion stay open. */
struct oa_svg_list_anim_ops {
    void *context;
    int (*anim_enabled)(void *list);
    uint32_t (*selected_item)(void *manager);
    void *(*item_row)(void *manager,uint32_t index);
    void (*get_rect)(void *row,uint32_t rect[4]);
    void (*read_cur)(void *list,uint32_t out[4]);
    void (*write_prev)(void *list,const uint32_t in[4]);
    void (*write_cur)(void *list,const uint32_t in[4]);
    const char *(*std_name)(void *list,uint32_t id);
    uint32_t (*anim_count)(void *container);
    void *(*anim_at)(void *container,uint32_t index);
    const char *(*anim_name)(void *anim);
    const char *(*anim_text)(void *anim);
    void *(*anim_controller)(void *owner);
    void *(*anim_owner)(void *list);
    void *(*anim_container)(void *list);
    void *(*anim_target)(void *list);
    void (*anim_start)(void *controller,void *target,float value);
    void (*ustr_new)(const char *s);
    uint32_t (*ustr_len)(const char *s);
    const char *(*ustr_sub)(const char *s,uint32_t start,uint32_t length);
    int (*ustr_equal)(const char *a,const char *b);
    void (*ustr_free)(const char *t);
};
int oa_svg_list_start_animation(const struct oa_svg_list_anim_ops *o,void *list,void *manager,float *started);
#endif
