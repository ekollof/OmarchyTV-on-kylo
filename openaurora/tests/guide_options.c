/* EPG event options dialog: record-151 geometry/height and option rules. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/guide_options.h"

int main(void)
{
    oa_guide_options_layout l;
    assert(oa_guide_options_layout_default(&l) == 0);
    assert(l.panel.x == 0 && l.panel.x1 == 1920 && l.panel.y1 == 1080);
    assert(l.bg_textbox.x == 610 && l.bg_textbox.y == 806 &&
           l.bg_textbox.x1 == 1310 && l.bg_textbox.y1 == 909);
    assert(l.item.x == 610 && l.item.y == 844 && l.item.x1 == 1310 &&
           l.item.y1 == 920);
    assert(l.title.x == 634 && l.title.y == 750 && l.title.x1 == 1320 &&
           l.title.y1 == 825);
    assert(l.item_pitch == 65 && l.item_height == 76 && l.bg_opacity == 7);
    assert(l.item_rgb[0] == 1 && l.focus_rgb[0] == 254 &&
           l.focus_text_rgb[0] == 126);
    assert(oa_guide_options_layout_default(NULL) == -EINVAL);

    /* calculateHeight: 65*n + 16 for record 151. */
    assert(oa_guide_options_height(&l, 0) == 16);
    assert(oa_guide_options_height(&l, 1) == 81);
    assert(oa_guide_options_height(&l, 3) == 211);
    assert(oa_guide_options_height(&l, 5) == 341);
    assert(oa_guide_options_height(NULL, 3) == -EINVAL);
    assert(oa_guide_options_height(&l, -1) == -EINVAL);

    oa_guide_rect r;
    assert(oa_guide_options_item_rect(&l, 0, 0, &r) == 0);
    assert(r.x == 610 && r.y == 844 && r.x1 == 1310 && r.y1 == 920);
    assert(oa_guide_options_item_rect(&l, 2, 0, &r) == 0);
    assert(r.y == 974 && r.y1 == 1050);
    assert(oa_guide_options_item_rect(&l, 2, -21, &r) == 0);
    assert(r.y == 953 && r.y1 == 1029);
    assert(oa_guide_options_item_rect(&l, -1, 0, &r) == -ERANGE);
    assert(oa_guide_options_item_rect(NULL, 0, 0, &r) == -EINVAL);

    struct oa_guide_options_inputs in;
    const char *keys[OA_GUIDE_OPTIONS_MAX];
    unsigned count = 99;

    /* Enhanced event with an info text: VIEW_CHANNEL + MORE_INFO. */
    memset(&in, 0, sizeof in);
    in.has_enhanced = 1;
    assert(oa_guide_options_build(&in, keys, 6, &count) == 0 && count == 2);
    assert(!strcmp(keys[0], "VIEW_CHANNEL"));
    assert(!strcmp(keys[1], "MORE_INFO"));

    /* Restart URL wins over on-demand watch; no-information hides MORE_INFO. */
    in.restart_url = 1;
    in.on_demand_available = 2;
    in.no_information = 1;
    assert(oa_guide_options_build(&in, keys, 6, &count) == 0 && count == 2);
    assert(!strcmp(keys[1], "RESTART"));

    /* No restart: on-demand available adds HLP_WATCH. */
    in.restart_url = 0;
    in.no_information = 0;
    assert(oa_guide_options_build(&in, keys, 6, &count) == 0 && count == 3);
    assert(!strcmp(keys[1], "HLP_WATCH"));
    assert(!strcmp(keys[2], "MORE_INFO"));

    /* Service player button is last. */
    in.player_available = 1;
    assert(oa_guide_options_build(&in, keys, 6, &count) == 0 && count == 4);
    assert(!strcmp(keys[3], "LAUNCH_SERVICE_PLAYER"));

    /* OnDemandProgram mode: HLP_WATCH then MORE_INFO then the player. */
    memset(&in, 0, sizeof in);
    in.on_demand_mode = 1;
    in.on_demand_available = 2;
    in.player_available = 1;
    assert(oa_guide_options_build(&in, keys, 6, &count) == 0 && count == 4);
    assert(!strcmp(keys[0], "VIEW_CHANNEL"));
    assert(!strcmp(keys[1], "HLP_WATCH"));
    assert(!strcmp(keys[2], "MORE_INFO"));
    assert(!strcmp(keys[3], "LAUNCH_SERVICE_PLAYER"));

    /* No enhanced event: MORE_INFO + player. */
    memset(&in, 0, sizeof in);
    in.player_available = 1;
    assert(oa_guide_options_build(&in, keys, 6, &count) == 0 && count == 3);
    assert(!strcmp(keys[1], "MORE_INFO"));
    assert(!strcmp(keys[2], "LAUNCH_SERVICE_PLAYER"));

    assert(oa_guide_options_build(&in, keys, 2, &count) == -EOVERFLOW);
    assert(oa_guide_options_build(NULL, keys, 6, &count) == -EINVAL);
    assert(oa_guide_options_build(&in, NULL, 6, &count) == -EINVAL);
    assert(oa_guide_options_build(&in, keys, 6, NULL) == -EINVAL);

    puts("ok guide options: record-151 items/height and option rules");
    return 0;
}
