/* ui_omarchy saved state: script-derived colors/layout and the last painted
 * screen size. The control probe decodes the same blob the reload handoff
 * carries; it is project diagnostic data, not a service contract. */
#ifndef OPENAURORA_UI_OMARCHY_H
#define OPENAURORA_UI_OMARCHY_H
#include <stdint.h>

#define OA_OMARCHY_STATE_MAGIC 0x4f414f53u /* "OAOS" */
#define OA_OMARCHY_STATE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    int32_t have_colors;
    int32_t bg[4];
    int32_t accent[3];
    int32_t have_layout;
    int32_t layout[4];
    int32_t have_screen;
    int32_t screen[2];
} oa_omarchy_state;

#endif
