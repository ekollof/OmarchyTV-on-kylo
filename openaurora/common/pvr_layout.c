/* Stock PVR list layout from DialogPlayList.svg (BinSVG 692). Pure data. */
#include "openaurora/pvr_layout.h"

#include <errno.h>
#include <string.h>

int oa_pvr_layout_default(oa_pvr_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_pvr_rect){ 0, 0, 1920, 1080 };
    out->header = (oa_pvr_rect){ 0, 0, 1921, 162 };
    out->subheader = (oa_pvr_rect){ 0, 162, 1920, 216 };
    out->title_name = (oa_pvr_rect){ 176, 164, 776, 209 };
    out->title_start = (oa_pvr_rect){ 808, 164, 1136, 209 };
    out->title_length = (oa_pvr_rect){ 1307, 164, 1561, 209 };
    out->list = (oa_pvr_rect){ 1, 216, 1911, 971 };
    out->row = (oa_pvr_rect){ 26, 222, 1886, 288 };
    out->name = (oa_pvr_rect){ 181, 230, 811, 275 };
    out->time = (oa_pvr_rect){ 815, 230, 1305, 275 };
    out->duration = (oa_pvr_rect){ 1307, 230, 1627, 275 };
    out->scrollbar = (oa_pvr_rect){ 1910, 217, 1920, 974 };
    out->used_space = (oa_pvr_rect){ 315, 1005, 775, 1045 };
    out->reserved_space = (oa_pvr_rect){ 779, 1005, 1287, 1045 };
    out->free_space = (oa_pvr_rect){ 1295, 1005, 1697, 1045 };
    out->space_band = (oa_pvr_rect){ 0, 971, 1920, 1080 };
    for (int i = 0; i < 5; i++)
        out->tabs[i] = (oa_pvr_rect){ 206 + i * 302, 101,
                                     506 + i * 302, 160 };
    out->dialog_title = (oa_pvr_rect){ -34, 8, 1449, 98 };
    out->clock = (oa_pvr_rect){ 1372, 26, 1910, 71 };
    out->row_pitch = 75;   /* shared full-screen-list step, not in record 692 */
    out->row_height = 66;

    const uint8_t panel[3] = { 220, 219, 218 };   /* #DCDBDA */
    const uint8_t header[3] = { 87, 43, 65 };     /* #481730 at 9/10 */
    const uint8_t subheader[3] = { 154, 153, 153 }; /* #010101 at 3/10 */
    const uint8_t text[3] = { 17, 17, 17 };        /* #111111 */
    const uint8_t title[3] = { 255, 255, 255 };
    const uint8_t focus[3] = { 254, 148, 25 };     /* theme focus */
    const uint8_t band[3] = { 72, 23, 48 };        /* #481730 inference */
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->header_rgb, header, sizeof header);
    memcpy(out->subheader_rgb, subheader, sizeof subheader);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->title_rgb, title, sizeof title);
    memcpy(out->focus_rgb, focus, sizeof focus);
    memcpy(out->space_band_rgb, band, sizeof band);
    return 0;
}

int oa_pvr_tab_at(const oa_pvr_layout *layout, int x, int y)
{
    if (!layout)
        return -1;
    for (int i = 0; i < 5; i++) {
        const oa_pvr_rect *r = &layout->tabs[i];
        if (x >= r->x && x < r->x1 && y >= r->y && y < r->y1)
            return i;
    }
    return -1;
}

int oa_pvr_row_rect(const oa_pvr_layout *layout, int index, oa_pvr_rect *out)
{
    if (!layout || !out || index < 0)
        return -EINVAL;
    if (layout->row_pitch <= 0)
        return -EINVAL;
    int y = layout->row.y + index * layout->row_pitch;
    int end = layout->list.y1 < layout->panel.y1 ? layout->list.y1
                                                 : layout->panel.y1;
    if (y < layout->row.y || y >= end)
        return -ERANGE;
    *out = layout->row;
    out->y = y;
    out->y1 = y + layout->row_height;
    return 0;
}

int oa_pvr_visible_rows(const oa_pvr_layout *layout)
{
    if (!layout || layout->row_pitch <= 0)
        return -EINVAL;
    if (layout->list.y1 <= layout->list.y)
        return 0;
    return (layout->list.y1 - layout->list.y) / layout->row_pitch;
}
