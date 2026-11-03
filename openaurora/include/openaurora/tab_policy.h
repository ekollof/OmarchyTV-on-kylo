#ifndef OPENAURORA_TAB_POLICY_H
#define OPENAURORA_TAB_POLICY_H
#include <stdint.h>
#include <stddef.h>
/* Raw inputs used by TabDialog::isTabAvailable. Do not infer them from a
 * screenshot or item count. network_profile_flag is constructor byte37a0,
 * MenuCapability connectivity availability from swprofile value==1;
 * from_launcher is constructor argument/byte4b7, not a guessed global mode.
 * Raw byte flags preserve the firmware XOR1/low-byte behavior. */
struct oa_tab_context {
    uint32_t hotel_osd_mode;
    uint8_t hotel_active,hotel_picture_flag,network_profile_flag;
    uint8_t from_launcher,in_media_browser,in_app;
};
/* Raw availability result (0..255), not a widget index. Firmware defaults
 * unknown tab IDs, including negative IDs, to available1. EINVAL leaves out
 * unchanged. Supported consumers must separately validate actual IDs0..4. */
int oa_tab_available(const struct oa_tab_context *context,int32_t tab,uint8_t *out);
/* Bit0..4 of available tabs. Missing widgets/descriptions, focus selection,
 * content validity and permission/password checks are separate. */
int oa_tab_available_mask(const struct oa_tab_context *context,uint8_t *out);
/* init requests focusSubMenu iff open_submenu is nonzero OR item>0. This is
 * a request, not permission to focus: content/password/privacy gates follow. */
int oa_tab_requests_content_focus(uint8_t open_submenu,int32_t item);
struct oa_tab_selection {
    uint8_t created_mask,count,selected_index;
    int32_t focused_tab,content_tab;
};
/* Initial, unmodified TabDialog group only: available/described bits0..4.
 * Missing requested tab leaves group index0; no children means focused_tab=-1
 * but content_tab retains constructor0. Does not authorize content creation or
 * focus, and does not model a later key/modal transition. Invalid inputs leave
 * out unchanged. */
int oa_tab_initial_selection(uint8_t available_mask,uint8_t described_mask,
                             int32_t requested_tab,struct oa_tab_selection *out);
/* Only immutable profile inputs, NOT mutable hotel activation or app state.
 * Firmware defaults connectivity0/hotel1, both enabled exactly when raw==1. */
struct oa_tab_profile {
    uint32_t connectivity_raw,hotel_raw;
    uint8_t connectivity_enabled,hotel_enabled,connectivity_present,hotel_present;
};
int oa_tab_profile_from_sw(const void *data,size_t size,struct oa_tab_profile *out);
#endif
