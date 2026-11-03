#ifndef OPENAURORA_SETTINGS_INPUT_H
#define OPENAURORA_SETTINGS_INPUT_H
#include <stdint.h>
enum oa_settings_route {
    OA_SD_MENU,OA_SD_RIGHT_LEFT,OA_SD_UP_DOWN,OA_SD_SELECT,OA_SD_QUIT,
    OA_SD_MUTE,OA_SD_DIGIT,OA_SD_GUIDE,OA_SD_BASE
};
struct oa_settings_dispatch_ops {
    void *context;
    uint32_t (*run)(void *,enum oa_settings_route,const void *);
    const void *message;
};
/* Concrete qualified SettingsDialog map; decoded immutable full-width type/ID.
 * MUTE has no message operand and always consumes. Other routes forward the
 * original borrowed event and propagate result truth (including MENU, unlike
 * the outer TabDialog map). Positive errno is separate from handled0/1. */
int oa_settings_dispatch(const struct oa_settings_dispatch_ops *,uint32_t type,uint32_t id,int *handled);
enum oa_settings_input_kind { OA_SI_UP_DOWN,OA_SI_SELECT,OA_SI_DIGIT,OA_SI_RIGHT_LEFT,OA_SI_MUTE };
enum oa_settings_query {
    OA_SI_ACTIVE_COUNT,OA_SI_HORIZONTAL,OA_SI_MIRROR,OA_SI_SELECTED_ID,
    OA_SI_ITEM_INDEX,OA_SI_ITEM_PRESENT,OA_SI_DESCRIPTION_SENTINEL,OA_SI_TEXT_EMPTY
};
enum oa_settings_action {
    OA_SI_LIST_KEY,OA_SI_NAVIGATE,OA_SI_UPDATE_DISPLAY,OA_SI_UPDATE_SETTINGS,
    OA_SI_ASSIGN_DESCRIPTION,OA_SI_ASSIGN_LABEL,OA_SI_ADD_MENU_INFO,OA_SI_TOGGLE_MUTE
};
struct oa_settings_input_ops {
    void *context;
    uint32_t (*query)(void *,enum oa_settings_query,uint32_t argument);
    void (*run)(void *,enum oa_settings_action,uint32_t argument,const void *);
    const void *message;
};
/* Ordered method bodies, not the list widget or navigateInList implementation.
 * Query ITEM_INDEX takes selected ID; ITEM_PRESENT/DESCRIPTION_SENTINEL take
 * raw signed32 index. Description/label actions also take that index, which
 * stays captured across callbacks even if selection changes. Sentinel tests
 * source BLANK-token pointer identity, not string equality; TEXT_EMPTY is queried
 * after the possible assignment. UPDATE_SETTINGS is the virtual+84 operation.
 * LIST_KEY borrows original message; NAVIGATE does too for UP_DOWN, but horizontal
 * translation carries NULL and needs a fresh event. Other actions carry NULL.
 * Caller supplies a valid item mapping, pins code/context/message throughout
 * this synchronous call and gates permissions/lifetimes/nonblocking work. No
 * pointer is retained and invoking an action proves no hardware/UI effect.
 * Invalid arguments: EINVAL, no callbacks/output write. Success: handled1. */
int oa_settings_input(const struct oa_settings_input_ops *,enum oa_settings_input_kind,uint32_t key,int *handled);
#endif
