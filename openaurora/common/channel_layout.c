/* Stock channel-list layout from DialogAdvancedChannelList.svg + p0/04.
 * Pure data/arithmetic; no UI or device I/O. */
#include "openaurora/channel_layout.h"

#include <errno.h>
#include <string.h>

int oa_channel_layout_default(oa_channel_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_channel_rect){ 0, 0, 780, 1080 };
    out->banner = (oa_channel_rect){ 0, 0, 780, 162 };
    out->title = (oa_channel_rect){ 51, 2, 698, 67 };
    out->filter_info = (oa_channel_rect){ 123, 50, 766, 95 };
    out->tabs[0] = (oa_channel_rect){ 12, 100, 262, 159 };
    out->tabs[1] = (oa_channel_rect){ 264, 100, 514, 159 };
    out->tabs[2] = (oa_channel_rect){ 516, 100, 766, 159 };
    out->list = (oa_channel_rect){ 19, 179, 769, 1090 };
    out->logo = (oa_channel_rect){ 610, 21, 766, 66 };
    out->row = (oa_channel_rect){ 19, 195, 767, 261 };
    out->row_pitch = 75;
    out->row_height = 66;
    out->row_text_height = 34;
    out->focus_text_height = 40;

    const uint8_t panel[3] = { 190, 30, 45 };      /* #BE1E2D */
    const uint8_t banner[3] = { 178, 29, 43 };     /* #811924 @ 2/10 */
    const uint8_t tab[3] = { 193, 71, 83 };        /* white @ 7/10 */
    const uint8_t tab_sel[3] = { 255, 255, 255 };
    const uint8_t focus[3] = { 254, 148, 25 };     /* #FE9419 */
    const uint8_t text[3] = { 255, 255, 255 };
    const uint8_t focus_text[3] = { 126, 0, 10 };  /* #7E000A */
    const uint8_t muted[3] = { 198, 198, 198 };    /* #C6C6C6 */
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->banner_rgb, banner, sizeof banner);
    memcpy(out->tab_rgb, tab, sizeof tab);
    memcpy(out->tab_selected_rgb, tab_sel, sizeof tab_sel);
    memcpy(out->focus_rgb, focus, sizeof focus);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->focus_text_rgb, focus_text, sizeof focus_text);
    memcpy(out->muted_rgb, muted, sizeof muted);
    return 0;
}

int oa_channel_tab_at(const oa_channel_layout *layout, int x, int y)
{
    if (!layout)
        return -1;
    for (int i = 0; i < 3; i++) {
        const oa_channel_rect *r = &layout->tabs[i];
        if (x >= r->x && x < r->x1 && y >= r->y && y < r->y1)
            return i;
    }
    return -1;
}

int oa_channel_row_rect(const oa_channel_layout *layout, int index,
                        oa_channel_rect *out)
{
    if (!layout || !out || index < 0)
        return -EINVAL;
    int y = layout->row.y + index * layout->row_pitch;
    if (y < layout->row.y || y >= layout->panel.y1)
        return -ERANGE;
    *out = layout->row;
    out->y = y;
    out->y1 = y + layout->row_height;
    return 0;
}

int oa_channel_visible_rows(const oa_channel_layout *layout)
{
    if (!layout || layout->row_pitch <= 0)
        return -EINVAL;
    if (layout->row.y >= layout->panel.y1)
        return 0;
    return (layout->panel.y1 - layout->row.y + layout->row_pitch - 1) /
           layout->row_pitch;
}
