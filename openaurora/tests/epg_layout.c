/* Stock EPG layout/timeline model: record-101 rects, list split, event ratio,
 * instant x, explicit row pitch and tab edges. Pure host test. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/epg_layout.h"

static void rect_eq(const oa_epg_rect *r, int x, int y, int x1, int y1)
{
    assert(r->x == x && r->y == y && r->x1 == x1 && r->y1 == y1);
}

int main(void)
{
    oa_epg_layout l;
    assert(oa_epg_layout_default(&l) == 0);

    /* DialogEPG.svg (record 101). */
    rect_eq(&l.panel, 0, 0, 1920, 1080);
    rect_eq(&l.header, 0, 0, 1920, 319);
    rect_eq(&l.under_list, 0, 319, 1920, 1080);
    rect_eq(&l.date_bar, 0, 264, 1920, 318);
    rect_eq(&l.dialog_title, 164, 21, 560, 75);
    rect_eq(&l.channel_name, 590, 9, 1434, 81);
    rect_eq(&l.event_title, 593, 84, 1437, 144);
    rect_eq(&l.event_time, 290, 149, 551, 194);
    rect_eq(&l.date, 44, 266, 368, 311);
    rect_eq(&l.time_l, 677, 266, 927, 311);
    rect_eq(&l.time_m, 1196, 266, 1446, 311);
    rect_eq(&l.time_r, 1713, 266, 1868, 311);
    rect_eq(&l.list, 3, 319, 1920, 1043);
    rect_eq(&l.service_list, 0, 319, 640, 1043);
    rect_eq(&l.tabs[0], 206, 198, 506, 257);
    rect_eq(&l.tabs[4], 1414, 198, 1714, 257);
    assert(l.interval_s == 7200);
    assert(l.panel_rgb[0] == 255 && l.header_rgb[0] == 72 &&
           l.date_bar_rgb[0] == 36 && l.under_rgb[0] == 190 &&
           l.list_rgb[0] == 197 && l.event_title_rgb[0] == 247);

    /* createServiceList split: list.x + service width + 1. */
    oa_epg_rect services, timeline;
    assert(oa_epg_list_split(&l.list, 640, &services, &timeline) == 0);
    rect_eq(&services, 3, 319, 644, 1043);
    rect_eq(&timeline, 644, 319, 1920, 1043);
    assert(oa_epg_list_split(&l.list, 0, &services, &timeline) == -EINVAL);
    assert(oa_epg_list_split(NULL, 640, &services, &timeline) == -EINVAL);
    oa_epg_rect wide;
    assert(oa_epg_list_split(&l.list, 5000, &services, &wide) == 0);
    assert(services.x1 == 1920 && wide.x == 1920 && wide.x1 == 1920);

    /* Event bars: width 1276, 7200 s window. */
    int x = -1, w = -1;
    assert(oa_epg_event_span(&timeline, 0, 7200, 0, 3600000, &x, &w) == 0);
    assert(x == 644 && w == 638);
    assert(oa_epg_event_span(&timeline, 0, 7200, 3600000, 7200000, &x, &w) == 0);
    assert(x == 1282 && w == 638);
    assert(oa_epg_event_span(&timeline, 0, 7200, -1800000, 1800000,
                             &x, &w) == 0);
    assert(x == 644 && w == 319);           /* clipped to the window start */
    assert(oa_epg_event_span(&timeline, 0, 7200, 0, 9000000, &x, &w) == 0);
    assert(x == 644 && w == 1276);          /* clamped to the box width */
    assert(oa_epg_event_span(&timeline, 0, 7200, 7000000, 9000000,
                             &x, &w) == 0);
    assert(x == 644 + 1240 && w == 36);
    assert(oa_epg_event_span(&timeline, 0, 7200, 7200000, 9000000,
                             &x, &w) == -ERANGE);
    /* A sub-pixel span rounds to zero width and is not visible. */
    assert(oa_epg_event_span(&timeline, 0, 7200, 60000, 61000, &x, &w) ==
           -ERANGE);
    assert(oa_epg_event_span(&timeline, 0, 0, 0, 1000, &x, &w) == -EINVAL);
    assert(oa_epg_event_span(&timeline, 0, 7200, 1000, 500, &x, &w) == -EINVAL);
    assert(oa_epg_event_span(NULL, 0, 7200, 0, 1000, &x, &w) == -EINVAL);

    /* The now indicator uses the same ratio and is outside the window at the
     * end boundary. */
    assert(oa_epg_instant_x(&timeline, 0, 7200, 3600000, &x) == 0);
    assert(x == 1282);
    assert(oa_epg_instant_x(&timeline, 0, 7200, 0, &x) == 0 && x == 644);
    assert(oa_epg_instant_x(&timeline, 0, 7200, 7200000, &x) == -ERANGE);
    assert(oa_epg_instant_x(&timeline, 0, 7200, -1, &x) == -ERANGE);
    assert(oa_epg_instant_x(&timeline, 0, 0, 0, &x) == -EINVAL);

    /* Focused event: first event whose end is after focus_ms, else the last
     * (ServiceTimelineBox::setFocusedEvent). */
    struct oa_epg_event ev[3];
    memset(ev, 0, sizeof ev);
    ev[0].start_ms = 0;      ev[0].end_ms = 1000;
    ev[1].start_ms = 2000;   ev[1].end_ms = 3000;
    ev[2].start_ms = 5000;   ev[2].end_ms = 6000;
    assert(oa_epg_focus_event(ev, 3, 0) == 0);
    assert(oa_epg_focus_event(ev, 3, 999) == 0);
    assert(oa_epg_focus_event(ev, 3, 1000) == 1);   /* first end > focus */
    assert(oa_epg_focus_event(ev, 3, 2500) == 1);
    assert(oa_epg_focus_event(ev, 3, 3000) == 2);
    assert(oa_epg_focus_event(ev, 3, 9000) == 2);   /* none left: last */
    assert(oa_epg_focus_event(ev, 0, 0) == -1);
    assert(oa_epg_focus_event(NULL, 3, 0) == -1);

    /* Visible rows always take an explicit pitch; none is implied. */
    assert(oa_epg_visible_rows(&l, 76) == 9);
    assert(oa_epg_visible_rows(&l, 85) == 8);
    assert(oa_epg_visible_rows(&l, 0) == -EINVAL);
    assert(oa_epg_visible_rows(NULL, 76) == -EINVAL);

    assert(oa_epg_tab_at(&l, 300, 200) == 0);
    assert(oa_epg_tab_at(&l, 1500, 200) == 4);
    assert(oa_epg_tab_at(&l, 506, 200) == -1);
    assert(oa_epg_tab_at(&l, 206, 257) == -1);
    assert(oa_epg_tab_at(NULL, 0, 0) == -1);

    /* serviceB{FA,UFA,FI,UFI}.svg (records 456..459) event styles. */
    struct oa_epg_event_style st;
    oa_epg_event_style(1, 1, &st);
    assert(st.fill[0] == 247 && st.time_fill[0] == 211 && st.text[0] == 126);
    oa_epg_event_style(1, 0, &st);
    assert(st.fill[0] == 237 && st.time_fill[0] == 216 && st.text[0] == 126);
    oa_epg_event_style(0, 0, &st);
    assert(st.fill[0] == 126 && st.time_fill[0] == 109 && st.text[0] == 255);
    oa_epg_event_style(0, 1, &st);
    assert(st.fill[0] == 126 && st.text[0] == 255);
    oa_epg_event_style(1, 1, NULL);      /* explicit no-op, not a crash */

    /* DialogEPG_synopsis.svg (record 102). */
    oa_epg_synopsis y;
    assert(oa_epg_synopsis_default(&y) == 0);
    rect_eq(&y.panel, 0, 0, 1920, 1080);
    rect_eq(&y.header, 0, 0, 1920, 264);
    rect_eq(&y.event_bar, 0, 264, 1920, 571);
    rect_eq(&y.event_image, 62, 299, 382, 479);
    rect_eq(&y.event_text, 429, 273, 1882, 512);
    rect_eq(&y.event_duration, 76, 472, 396, 517);
    rect_eq(&y.date, 44, 518, 368, 562);
    rect_eq(&y.time_m, 1196, 519, 1446, 563);
    rect_eq(&y.icon_right, 1866, 516, 1920, 570);
    rect_eq(&y.list, 0, 570, 1920, 1077);
    rect_eq(&y.service_list, 0, 572, 640, 994);
    rect_eq(&y.tabs[0], 206, 198, 506, 257);
    assert(y.interval_s == 7200);
    assert(y.service_rgb[0] == 197 && y.service_rgb[1] == 201);
    assert(y.event_bar_rgb[0] == 36 && y.header_rgb[0] == 72);
    assert(oa_epg_synopsis_default(NULL) == -EINVAL);

    puts("ok epg layout: record-101/102 rects, list split, 7200 s event ratio, "
         "instant x, pitch, tabs, event styles and focus");
    return 0;
}
