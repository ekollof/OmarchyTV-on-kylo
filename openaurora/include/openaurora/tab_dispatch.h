#ifndef OPENAURORA_TAB_DISPATCH_H
#define OPENAURORA_TAB_DISPATCH_H
#include "openaurora/tab_prehandle.h"
/* TabDialog's fallback handleMessage map. BASE_MENU is the qualified
 * SettingsDialog handler, not TabDialog's same-named wrapper. */
enum oa_tab_dispatch_target {
    OA_TD_BASE_MENU,OA_TD_UP_DOWN,OA_TD_RIGHT_LEFT,OA_TD_VOLUME,
    OA_TD_QUIT_IGNORE,OA_TD_GUIDE,OA_TD_DIGIT,OA_TD_REMOVED,OA_TD_BASE
};
struct oa_tab_dispatch_ops {
    void *context;
    uint32_t (*run)(void *,enum oa_tab_dispatch_target,const void *message);
    const void *message;
};
/* Decoded immutable UIAnyMessage::type/id values, full width. Only type0x1004
 * can enter a key route. Every route forwards the original borrowed message.
 * Returns errno separately from normalized handled0/1; BASE_MENU is always
 * consumed regardless of its result. EINVAL runs no callback/output write.
 * Context/code/message must remain pinned through this synchronous call;
 * actions must be nonblocking and supply their own permission/lifetime gates. */
int oa_tab_dispatch(const struct oa_tab_dispatch_ops *,uint32_t type,uint32_t id,int *handled);
/* Already decoded key delivery: pre-handler first, fallback only if unhandled.
 * Both operation sets must refer to the same borrowed message (NULL allowed).
 * Invalid arguments execute neither stage. A pre-handler error is propagated
 * without fallback or changing handled; its earlier actions are not rolled back.
 * This does not dequeue events or grant permission to execute UI/hardware work. */
int oa_tab_handle_key(const struct oa_tab_pre_ops *,const struct oa_tab_dispatch_ops *,uint32_t key,int *handled);
#endif
