/* Stock PVR list layout from DialogPlayList.svg (BinSVG 692). Pure test. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/pvr_layout.h"

static void rect_eq(const oa_pvr_rect *r, int x, int y, int x1, int y1)
{
    assert(r->x == x && r->y == y && r->x1 == x1 && r->y1 == y1);
}

int main(void)
{
    oa_pvr_layout l;
    assert(oa_pvr_layout_default(&l) == 0);
    rect_eq(&l.panel, 0, 0, 1920, 1080);
    rect_eq(&l.header, 0, 0, 1921, 162);
    rect_eq(&l.subheader, 0, 162, 1920, 216);
    rect_eq(&l.title_name, 176, 164, 776, 209);
    rect_eq(&l.title_start, 808, 164, 1136, 209);
    rect_eq(&l.title_length, 1307, 164, 1561, 209);
    rect_eq(&l.list, 1, 216, 1911, 971);
    rect_eq(&l.row, 26, 222, 1886, 288);
    rect_eq(&l.name, 181, 230, 811, 275);
    rect_eq(&l.time, 815, 230, 1305, 275);
    rect_eq(&l.duration, 1307, 230, 1627, 275);
    rect_eq(&l.scrollbar, 1910, 217, 1920, 974);
    rect_eq(&l.used_space, 315, 1005, 775, 1045);
    rect_eq(&l.reserved_space, 779, 1005, 1287, 1045);
    rect_eq(&l.free_space, 1295, 1005, 1697, 1045);
    rect_eq(&l.tabs[0], 206, 101, 506, 160);
    rect_eq(&l.tabs[4], 1414, 101, 1714, 160);
    assert(l.row_pitch == 75 && l.row_height == 66);
    assert(l.panel_rgb[0] == 220 && l.header_rgb[0] == 87 &&
           l.subheader_rgb[0] == 154 && l.text_rgb[0] == 17);

    assert(oa_pvr_tab_at(&l, 300, 120) == 0);
    assert(oa_pvr_tab_at(&l, 1500, 120) == 4);
    assert(oa_pvr_tab_at(&l, 506, 120) == -1);
    assert(oa_pvr_tab_at(&l, 206, 160) == -1);
    assert(oa_pvr_tab_at(NULL, 0, 0) == -1);

    oa_pvr_rect r;
    assert(oa_pvr_row_rect(&l, 0, &r) == 0);
    rect_eq(&r, 26, 222, 1886, 288);
    assert(oa_pvr_row_rect(&l, 1, &r) == 0);
    rect_eq(&r, 26, 297, 1886, 363);
    assert(oa_pvr_row_rect(&l, 9, &r) == 0);
    rect_eq(&r, 26, 897, 1886, 963);
    assert(oa_pvr_row_rect(&l, 10, &r) == -ERANGE);
    assert(oa_pvr_row_rect(&l, -1, &r) == -EINVAL);
    assert(oa_pvr_row_rect(NULL, 0, &r) == -EINVAL);
    assert(oa_pvr_row_rect(&l, 0, NULL) == -EINVAL);
    assert(oa_pvr_visible_rows(&l) == 10);

    oa_pvr_layout bad = l;
    bad.row_pitch = 0;
    assert(oa_pvr_visible_rows(&bad) == -EINVAL);
    assert(oa_pvr_layout_default(NULL) == -EINVAL);

    /* DialogPlayList_synopsis.svg (record 693). */
    oa_pvr_synopsis y;
    assert(oa_pvr_synopsis_default(&y) == 0);
    rect_eq(&y.panel, 0, 0, 1920, 1080);
    rect_eq(&y.header, 0, 0, 1921, 162);
    rect_eq(&y.top_band, 0, 162, 1920, 353);
    rect_eq(&y.subheader, 0, 353, 1920, 407);
    rect_eq(&y.synopsis_title, 82, 173, 1372, 223);
    rect_eq(&y.synopsis_text, 143, 204, 1855, 326);
    rect_eq(&y.title_name, 131, 354, 731, 399);
    rect_eq(&y.title_start, 811, 354, 1299, 399);
    rect_eq(&y.title_length, 1307, 354, 1636, 399);
    rect_eq(&y.list, 1, 404, 1911, 982);
    rect_eq(&y.row, 26, 412, 1886, 478);
    rect_eq(&y.scrollbar, 1910, 404, 1920, 984);
    rect_eq(&y.used_space, 315, 1001, 775, 1041);
    rect_eq(&y.reserved_space, 779, 1001, 1287, 1041);
    rect_eq(&y.free_space, 1295, 1001, 1697, 1041);
    rect_eq(&y.space_band, 0, 982, 1920, 1080);
    rect_eq(&y.tabs[0], 206, 101, 506, 160);
    assert(y.row_pitch == 75 && y.row_height == 66);
    assert(y.header_rgb[0] == 87 && y.band_rgb[0] == 72 &&
           y.text_rgb[0] == 17 && y.panel_rgb[0] == 220);
    assert(oa_pvr_synopsis_default(NULL) == -EINVAL);

    puts("ok pvr layout: record-692/693 rects, 75/66 rows, 5 text tabs and colours");
    return 0;
}
