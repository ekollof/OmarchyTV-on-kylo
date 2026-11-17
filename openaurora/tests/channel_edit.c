/* Channel edit-options model: record-100 layout, switch order and visibility. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/channel_edit.h"

int main(void)
{
    oa_channel_edit_layout l;
    assert(oa_channel_edit_layout_default(&l) == 0);
    assert(l.panel.x == 0 && l.panel.y == 35 && l.panel.x1 == 780 &&
           l.panel.y1 == 1080);
    assert(l.strip.x == 0 && l.strip.y == 160 && l.strip.x1 == 142);
    assert(l.content.x == 142 && l.content.y == 160 && l.content.x1 == 782);
    assert(l.list.x == 152 && l.list.y == 211 && l.list.x1 == 768 &&
           l.list.y1 == 1059);
    assert(l.row_pitch == 70 && l.row_height == 66 && l.visible_rows == 12);
    assert(l.panel_rgb[0] == 255 && l.strip_rgb[0] == 103 &&
           l.content_rgb[0] == 63 && l.focus_rgb[0] == 254);
    assert(oa_channel_edit_layout_default(NULL) == -EINVAL);

    const oa_channel_edit_option *c = oa_channel_edit_catalogue();
    assert(c && OA_CHANNEL_EDIT_OPTIONS == 8);
    assert(!strcmp(c[0].key, "ADD_TO_HOME") && c[0].case_id == 0);
    assert(!strcmp(c[1].key, "MOVE_TO_POSITION") && c[1].case_id == 1);
    assert(c[2].case_id == 2 && c[3].case_id == 3 && c[7].case_id == 7);

    unsigned out[8], count = 0;
    oa_channel_edit_inputs in = {
        .single_selection = 1, .multiple_selection = 0,
        .name_editable = 1, .deletable = 1, .at_home = 0, .rovi = 0
    };
    assert(oa_channel_edit_visible(&in, out, 8, &count) == 0 && count == 7);
    assert(out[0] == 0 && out[1] == 2 && out[2] == 3 && out[3] == 4 &&
           out[4] == 5 && out[5] == 6 && out[6] == 7);

    in.single_selection = 0;
    in.multiple_selection = 1;
    assert(oa_channel_edit_visible(&in, out, 8, &count) == 0 && count == 6);
    assert(out[0] == 1 && out[1] == 2 && out[5] == 7);

    in.deletable = 0;
    assert(oa_channel_edit_visible(&in, out, 8, &count) == 0 && count == 5);
    in.deletable = 1;
    in.rovi = 1;
    assert(oa_channel_edit_visible(&in, out, 8, &count) == 0 && count == 5);
    in.rovi = 0;

    in.single_selection = 1;
    in.multiple_selection = 0;
    in.at_home = 1;
    assert(oa_channel_edit_visible(&in, out, 8, &count) == 0 && count == 6);
    assert(out[0] == 2);

    assert(oa_channel_edit_visible(&in, out, 3, &count) == -EOVERFLOW);
    assert(oa_channel_edit_visible(NULL, out, 8, &count) == -EINVAL);
    assert(oa_channel_edit_visible(&in, NULL, 8, &count) == -EINVAL);
    assert(oa_channel_edit_visible(&in, out, 8, NULL) == -EINVAL);

    assert(!strcmp(oa_channel_edit_label(2, 0), "DELETE"));
    assert(!strcmp(oa_channel_edit_label(2, 1), "REMOVE"));
    assert(!strcmp(oa_channel_edit_label(5, 0), "HLP_FAVOURITE_ADD_REMOVE"));
    assert(oa_channel_edit_label(99, 0) == NULL);

    puts("ok channel edit: record-100 layout, switch order and visibility");
    return 0;
}
