#ifndef OPENAURORA_TAB_PREHANDLE_H
#define OPENAURORA_TAB_PREHANDLE_H
#include <stdint.h>
/* TabDialog pre-handler. Query callbacks are pure current-state reads.
 * All callbacks/context/message remain owned and pinned by the caller during
 * this synchronous call. No callback/message is retained. Actions requesting
 * focus/image/lifecycle do not establish permission or hardware success. */
enum oa_tab_pre_query {
    OA_TP_VERTICAL_TABS,OA_TP_IN_MEDIA,OA_TP_GROUP_HAS_FOCUS,OA_TP_GROUP_ID,
    OA_TP_CONTENT_TAB,OA_TP_PRESENT,OA_TP_FOCUSED,OA_TP_CIRCULAR,
    OA_TP_BUTTONS_ACTIVE,OA_TP_BACK_HANDLED,OA_TP_INDEX,OA_TP_SMALLEST,
    OA_TP_BIGGEST,OA_TP_TAB_JUMP
};
enum oa_tab_pre_action {
    OA_TP_SET_CONTENT_TAB,OA_TP_CLOSE_MODALS,OA_TP_POST_KEY,OA_TP_REQUEST_IMAGE,
    OA_TP_RELEASE_IMAGE,OA_TP_UNFOCUS,OA_TP_HIDE,OA_TP_END_DIALOG,
    OA_TP_BUTTONS,OA_TP_SELECT_CHILD,OA_TP_CONTENT_FOCUS,OA_TP_DIALOG_FOCUS,
    OA_TP_PUSH_FRONT_KEY,OA_TP_CONTENT_KEY,OA_TP_BASE_PREHANDLE,
    OA_TP_SET_ITEM,OA_TP_SHOW,OA_TP_FOCUS,OA_TP_RIGHT_LEFT,OA_TP_GROUP_KEY,
    OA_TP_UPDATE_TITLE
};
struct oa_tab_pre_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_tab_pre_query);
    uint32_t (*run)(void *,enum oa_tab_pre_action,uint32_t,const void *);
    const void *message;
};
/* Full ICE key and original borrowed event. CONTENT_KEY/BASE_PREHANDLE forward
 * the original message; synthesized keys carry NULL and require a fresh owned
 * event from the backend. POST/PUSH use the supplied key; REQUEST_IMAGE means
 * the source request operands0/1/1, not a harmless loader/query operation.
 * Return0 plus normalized handled0/1; EINVAL runs no callback. ENODATA prevents
 * a child dereference in source-invalid missing-content states, possibly after
 * earlier actions. Such states are not native parity acceptance. */
int oa_tab_prehandle(const struct oa_tab_pre_ops *,uint32_t ice_key,int *handled);
#endif
