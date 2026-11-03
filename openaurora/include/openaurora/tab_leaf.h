#ifndef OPENAURORA_TAB_LEAF_H
#define OPENAURORA_TAB_LEAF_H
#include <stdint.h>
enum oa_tab_leaf_kind {
    OA_TL_VOLUME,OA_TL_DIGIT,OA_TL_MENU,OA_TL_QUIT,OA_TL_GUIDE,OA_TL_REMOVED
};
enum oa_tab_leaf_query {
    OA_TL_CONTENT_PRESENT,OA_TL_CONTENT_SHOWING,OA_TL_IN_MEDIA,OA_TL_DIGITS
};
enum oa_tab_leaf_action {
    OA_TL_CONTENT_KEY,OA_TL_SET_DIGITS,OA_TL_END_DIALOG,OA_TL_SVG_END_DIALOG,
    OA_TL_RESET_PICTURE_INDEX,OA_TL_PUSH_KEY,OA_TL_PUSH_NAVIGATION
};
struct oa_tab_leaf_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_tab_leaf_query);
    void (*run)(void *,enum oa_tab_leaf_action,uint32_t,const void *message);
    const void *message;
};
/* TabDialog volume/digit/removal and qualified SettingsDialog menu/quit/guide
 * bodies. Only CONTENT_KEY borrows the original message; PUSH_KEY/NAVIGATION
 * require fresh owned events. END_DIALOG is virtual dispatch, not a direct
 * generic destruction call; digit results use the distinct qualified
 * SVG_END_DIALOG action instead. DIGITS is the raw32-bit rolling accumulator;
 * SET_DIGITS stores its signed-remainder bit pattern before any end action.
 * Returns errno separately from normalized handled0/1. Invalid arguments run
 * no callbacks; ENODATA guards absent volume content (outside native valid-tree
 * parity). No pointer is retained. Caller pins context/code/message and supplies
 * permission/lifecycle gates and nonblocking actions; an action isn't proof
 * of a UI/hardware effect. Only an already selected handler may be invoked. */
int oa_tab_leaf(const struct oa_tab_leaf_ops *,enum oa_tab_leaf_kind,uint32_t ice_key,int *handled);
#endif
