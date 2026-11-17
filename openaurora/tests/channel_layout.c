/* Stock channel-list layout model: resource 80 rects, capture row metrics and
 * colors. Pure host test; no UI or device. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/channel_layout.h"

static void rect_eq(const oa_channel_rect *r, int x, int y, int x1, int y1)
{
    assert(r->x == x && r->y == y && r->x1 == x1 && r->y1 == y1);
}

int main(void)
{
    oa_channel_layout l;
    assert(oa_channel_layout_default(&l) == 0);

    /* DialogAdvancedChannelList.svg (record 80) rectangles. */
    rect_eq(&l.panel, 0, 0, 780, 1080);
    rect_eq(&l.banner, 0, 0, 780, 162);
    rect_eq(&l.title, 51, 2, 698, 67);
    rect_eq(&l.filter_info, 123, 50, 766, 95);
    rect_eq(&l.tabs[0], 12, 100, 262, 159);
    rect_eq(&l.tabs[1], 264, 100, 514, 159);
    rect_eq(&l.tabs[2], 516, 100, 766, 159);
    rect_eq(&l.list, 19, 179, 769, 1090);
    rect_eq(&l.logo, 610, 21, 766, 66);

    /* p0/04 capture: first row surface and pitch, focus colours. */
    rect_eq(&l.row, 19, 195, 767, 261);
    assert(l.row_pitch == 75 && l.row_height == 66);
    assert(l.row_text_height == 34 && l.focus_text_height == 40);
    assert(l.panel_rgb[0] == 190 && l.panel_rgb[1] == 30 && l.panel_rgb[2] == 45);
    assert(l.banner_rgb[0] == 178 && l.banner_rgb[1] == 29 &&
           l.banner_rgb[2] == 43);
    assert(l.tab_rgb[0] == 193 && l.tab_selected_rgb[0] == 255);
    assert(l.focus_rgb[0] == 254 && l.focus_rgb[1] == 148 &&
           l.focus_rgb[2] == 25);
    assert(l.text_rgb[0] == 255 && l.focus_text_rgb[0] == 126 &&
           l.focus_text_rgb[2] == 10);

    /* Tab hit testing uses exclusive x1 and the 2 px inter-tab gaps. */
    assert(oa_channel_tab_at(&l, 137, 129) == 0);
    assert(oa_channel_tab_at(&l, 389, 129) == 1);
    assert(oa_channel_tab_at(&l, 641, 129) == 2);
    assert(oa_channel_tab_at(&l, 12, 100) == 0);
    assert(oa_channel_tab_at(&l, 261, 158) == 0);
    assert(oa_channel_tab_at(&l, 262, 129) == -1);
    assert(oa_channel_tab_at(&l, 263, 129) == -1);
    assert(oa_channel_tab_at(&l, 264, 129) == 1);
    assert(oa_channel_tab_at(&l, 766, 129) == -1);
    assert(oa_channel_tab_at(&l, 137, 159) == -1);
    assert(oa_channel_tab_at(NULL, 0, 0) == -1);

    /* Row arithmetic: 75 px pitch, 66 px row, 12 starts inside the panel.
     * Row 11 extends past 1080 and is the caller's clip responsibility. */
    oa_channel_rect r;
    assert(oa_channel_row_rect(&l, 0, &r) == 0);
    rect_eq(&r, 19, 195, 767, 261);
    assert(oa_channel_row_rect(&l, 1, &r) == 0);
    rect_eq(&r, 19, 270, 767, 336);
    assert(oa_channel_row_rect(&l, 11, &r) == 0);
    rect_eq(&r, 19, 1020, 767, 1086);
    assert(oa_channel_row_rect(&l, 12, &r) == -ERANGE);
    assert(oa_channel_row_rect(&l, -1, &r) == -EINVAL);
    assert(oa_channel_row_rect(NULL, 0, &r) == -EINVAL);
    assert(oa_channel_row_rect(&l, 0, NULL) == -EINVAL);
    assert(oa_channel_visible_rows(&l) == 12);

    oa_channel_layout bad = l;
    bad.row_pitch = 0;
    assert(oa_channel_visible_rows(&bad) == -EINVAL);
    assert(oa_channel_layout_default(NULL) == -EINVAL);

    puts("ok channel layout: record-80 rects, 75/66 row metrics, tab gaps and "
         "capture colours");
    return 0;
}
