#ifndef OPENAURORA_TAB_EVENTS_H
#define OPENAURORA_TAB_EVENTS_H
#include <stdint.h>
/* Ordered TabDialog handler actions, separated from renderer/content ownership.
 * Query state when requested, rather than capturing it before actions: updating
 * a list can change button activation or content presence in the same call.
 * Context/code must remain alive through each synchronous, nonblocking call.
 * No callback is retained, no action result implies successful permission/focus.
 * These are onUpDownPress/onRightLeftPress only, NOT the pre-handler/router. */
enum oa_tab_query { OA_TAB_OPEN_SUBMENU,OA_TAB_BUTTONS_ACTIVE,OA_TAB_CONTENT_PRESENT };
enum oa_tab_action {
    OA_TAB_SET_ITEM,OA_TAB_UPDATE_ACTIVE_LIST,OA_TAB_SET_OPEN_SUBMENU,
    OA_TAB_HIDE_CONTENT,OA_TAB_GROUP_KEY,OA_TAB_UPDATE_TITLE,
    OA_TAB_SHOW_CONTENT,OA_TAB_FOCUS_CONTENT
};
struct oa_tab_event_ops {
    void *context;
    uint32_t (*query)(void *context,enum oa_tab_query query);
    void (*action)(void *context,enum oa_tab_action action,uint32_t argument,const void *message);
    const void *message; /* borrowed original event; forwarded only to GROUP_KEY */
};
/* ICE codes, never eight-bit RC/UART codes. Valid calls return0 (consumed;
 * corresponding native methods always return1). EINVAL for missing ops/callbacks
 * runs no callback. Caller decides when the full key router selects a handler.
 * Actions may reenter backend state, but must not unload this call's owner. */
int oa_tab_event_up_down(const struct oa_tab_event_ops *ops,uint32_t ice_key);
int oa_tab_event_right_left(const struct oa_tab_event_ops *ops,uint32_t ice_key);
#endif
