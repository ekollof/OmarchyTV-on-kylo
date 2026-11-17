#ifndef OPENAURORA_GUIDE_OPTIONS_H
#define OPENAURORA_GUIDE_OPTIONS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * EPG event options dialog, recovered from GuideEventOptionsDialog and
 * OptionsListDialog (aurora MD5 64c252340d3af6c5d66072611b7bad7a).
 *
 * GuideEventOptionsDialog::getDialogName returns "GuideEventOptions";
 * OptionsListDialog::getDialogName returns "OptionsList", whose theme resource
 * is DialogOptionsList.svg (BinSVG 151):
 *   panel 0,0,1920,1080 white
 *   bgTextbox 610,806,1310,909 (white at 7/10)
 *   itemPos   610,844,1310,920 (#010101, first item)
 *   dialogTitle 634,750,1320,825
 * OptionsListDialog::calculateItemPosition 0x00ea963c: item i is
 *   itemPos.x, itemPos.y + i*(bgTextbox.y1 - itemPos.y),
 *   itemPos.x1, itemPos.y1 + i*(bgTextbox.y1 - itemPos.y)
 * (pitch 65, height 76). calculateHeight 0x00ea9700:
 *   (bgTextbox.y1 - itemPos.y)*count + (itemPos.y - bgTextbox.y) +
 *   (bgTextbox.y1 - itemPos.y1)*2 = 65*count + 16 for record 151.
 * OptionsListDialog::reposition 0x00ea9894 shifts the whole dialog by
 *   base.y1 - (height + dialogTitle.y1), but base is a runtime-initialized
 *   screen rect (BSS pointer 0x1a1a464); the absolute anchor is not pinned
 *   here, so callers pass an explicit offset_y (0 = resource coordinates).
 *
 * GuideEventOptionsDialog::init 0x00ea5919 adds, in order:
 *   VIEW_CHANNEL always;
 *   enhanced event: RESTART when getRestartURL() != 0, else HLP_WATCH when
 *     onDemandAvailable() == 2; MORE_INFO when getNoInformationAvailable()==0;
 *   no enhanced event: MORE_INFO when its first vtable call != 0;
 *   LAUNCH_SERVICE_PLAYER through addServicePlayerButton 0x00ea572c when not
 *     in OnDemandProgram mode and the metadata provider reports a player.
 * The OnDemandProgram branch adds HLP_WATCH when available, then MORE_INFO,
 * then the service player button. The label keys resolve through
 * UIResourceManager::lookupString (dut "Zender kijken", "Restart",
 * "Bekijken", "Meer informatie", "Player lanceren").
 */
typedef struct {
    int x, y, x1, y1;
} oa_guide_rect;

typedef struct {
    oa_guide_rect panel;       /* 0,0,1920,1080 */
    oa_guide_rect bg_textbox;  /* 610,806,1310,909 */
    oa_guide_rect item;        /* 610,844,1310,920, first item */
    oa_guide_rect title;       /* 634,750,1320,825 */
    int item_pitch;            /* 65 */
    int item_height;           /* 76 */
    int bg_opacity;            /* 7 of 10 */
    unsigned char panel_rgb[3], bg_rgb[3], item_rgb[3];
    unsigned char focus_rgb[3], text_rgb[3], focus_text_rgb[3];
} oa_guide_options_layout;

struct oa_guide_options_inputs {
    int has_enhanced;          /* EnhancedServiceEvent present */
    int restart_url;           /* getRestartURL() != 0 */
    int on_demand_available;   /* OnDemandEvent::onDemandAvailable() == 2 */
    int no_information;        /* getNoInformationAvailable() != 0 */
    int player_available;      /* metadata provider player check */
    int on_demand_mode;        /* constructed with an OnDemandProgram */
};

#define OA_GUIDE_OPTIONS_MAX 6

/* Fill the record-151 defaults. -EINVAL for NULL. */
int oa_guide_options_layout_default(oa_guide_options_layout *out);

/* The shipped height formula; returns -EINVAL for NULL and 0 for count<0. */
int oa_guide_options_height(const oa_guide_options_layout *layout, int count);

/* Item i rect with the vendor reposition offset_y added to every edge.
 * Returns 0, -EINVAL for NULL/mismatched layout or -ERANGE for i<0. */
int oa_guide_options_item_rect(const oa_guide_options_layout *layout, int i,
                               int offset_y, oa_guide_rect *out);

/* Build the visible option keys in vendor order. `cap` is the caller's array
 * size; *count is the full visible count. -EINVAL for NULL, -EOVERFLOW when
 * cap is too small. */
int oa_guide_options_build(const struct oa_guide_options_inputs *in,
                           const char **keys, unsigned cap, unsigned *count);

#ifdef __cplusplus
}
#endif

#endif
