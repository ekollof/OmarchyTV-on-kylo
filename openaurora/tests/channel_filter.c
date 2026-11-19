/* Channel filter/sort popup model: record-636 rects, item catalogue and
 * navigation. Pure host test. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/channel_filter.h"

int main(void)
{
    oa_channel_filter_layout l;
    assert(oa_channel_filter_layout_default(&l) == 0);
    assert(l.panel.x == 60 && l.panel.y == 117 && l.panel.x1 == 666 &&
           l.panel.y1 == 467);
    assert(l.row_pitch == 70 && l.row_height == 66 && l.visible_rows == 5);
    assert(l.panel_rgb[0] == 73 && l.border_rgb[0] == 220 &&
           l.focus_rgb[0] == 254 && l.focus_text_rgb[0] == 126);
    /* Live render (fb1 capture 2026-11-19): record 636's rects are layout
     * metadata; the shared list box paints at 142,160..780,1080. */
    assert(l.render_panel.x == 142 && l.render_panel.y == 160 &&
           l.render_panel.x1 == 780 && l.render_panel.y1 == 1080);
    assert(l.render_focus.x == 152 && l.render_focus.y == 228 &&
           l.render_focus.x1 == 766 && l.render_focus.y1 == 293);
    assert(l.render_row_pitch == 75 && l.render_rows == 10);
    assert(l.render_panel_rgb[0] == 63 && l.render_panel_rgb[1] == 11 &&
           l.render_panel_rgb[2] == 19);
    assert(oa_channel_filter_layout_default(NULL) == -EINVAL);

    const oa_channel_filter_item *items = oa_channel_filter_catalogue();
    assert(items);
    assert(OA_CHANNEL_FILTER_ITEMS == 10);
    assert(!strcmp(items[0].key, "CHANNEL_TYPE") &&
           items[0].value_count == 6);
    assert(!strcmp(items[0].values[0], "ALL"));
    assert(!strcmp(items[0].values[5], "ANALOG_ONLY"));
    assert(!strcmp(items[1].key, "FILTER_TV_RADIO_LABEL") &&
           items[1].value_count == 3);
    assert(!strcmp(items[1].values[2], "TEXT_ONLY"));
    assert(!strcmp(items[2].key, "FREE_PAY_LABEL") &&
           items[2].value_count == 3);
    assert(!strcmp(items[2].values[0], "Alles"));
    assert(!strcmp(items[2].values[2], "FILTER_ENCRYPTED"));
    assert(!strcmp(items[3].key, "FILTER_LETTER_LABEL") &&
           items[3].value_count == 2);
    assert(!strcmp(items[4].key, "SORT_LABEL") &&
           items[4].value_count == 2);
    assert(!strcmp(items[4].values[0], "SORT_NUMERIC"));
    /* Live-only rows (fb1 capture 2026-11-19), literals pending i18n keys. */
    assert(!strcmp(items[5].key, "UHD/HD/SD") &&
           items[5].value_count == 5);
    assert(!strcmp(items[6].key, "Favorieten") &&
           !strcmp(items[6].values[0], "Geen"));
    assert(!strcmp(items[7].key, "Profielopties"));
    assert(!strcmp(items[8].key, "Satellietlijst"));
    assert(!strcmp(items[9].key, "Reset") && items[9].value_count == 0 &&
           items[9].values == NULL);

    oa_channel_filter f;
    assert(oa_channel_filter_default(&f) == 0);
    assert(f.selected == 0 && f.value[0] == 0);
    assert(oa_channel_filter_move(&f, 1) == 1);
    assert(oa_channel_filter_move(&f, 10) == 9);
    assert(oa_channel_filter_move(&f, -10) == 0);
    assert(oa_channel_filter_move(NULL, 1) == -EINVAL);

    assert(oa_channel_filter_change(&f, 1) == 1);       /* DVB_AERIAL_ONLY */
    assert(oa_channel_filter_change(&f, -1) == 0);      /* ALL */
    assert(oa_channel_filter_change(&f, -1) == 5);      /* ANALOG_ONLY wrap */
    assert(oa_channel_filter_change(&f, 1) == 0);       /* ALL wrap */
    f.selected = 3;                                     /* Alles, A-Z */
    assert(oa_channel_filter_change(&f, 1) == 1);
    f.selected = 4;
    assert(oa_channel_filter_change(&f, 1) == 1);       /* ALPHABETIC */
    f.selected = 9;
    assert(oa_channel_filter_change(&f, 1) == -EINVAL); /* Reset: no value */
    assert(oa_channel_filter_change(NULL, 1) == -EINVAL);

    puts("ok channel filter: record-636 popup, 10-row live catalogue, move/change");
    return 0;
}
