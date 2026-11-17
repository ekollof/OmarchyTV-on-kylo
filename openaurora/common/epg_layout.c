/* Stock EPG timeline layout and event arithmetic. Pure; no UI or device I/O. */
#include "openaurora/epg_layout.h"

#include <errno.h>
#include <string.h>

int oa_epg_layout_default(oa_epg_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_epg_rect){ 0, 0, 1920, 1080 };
    out->header = (oa_epg_rect){ 0, 0, 1920, 319 };
    out->under_list = (oa_epg_rect){ 0, 319, 1920, 1080 };
    out->date_bar = (oa_epg_rect){ 0, 264, 1920, 318 };
    out->dialog_title = (oa_epg_rect){ 164, 21, 560, 75 };
    out->channel_name = (oa_epg_rect){ 590, 9, 1434, 81 };
    out->event_title = (oa_epg_rect){ 593, 84, 1437, 144 };
    out->service_logo = (oa_epg_rect){ 204, 43, 354, 193 };
    out->channel_number = (oa_epg_rect){ 358, 65, 538, 137 };
    out->event_time = (oa_epg_rect){ 290, 149, 551, 194 };
    out->clock = (oa_epg_rect){ 1175, 149, 1434, 194 };
    out->date = (oa_epg_rect){ 44, 266, 368, 311 };
    out->time_l = (oa_epg_rect){ 677, 266, 927, 311 };
    out->time_m = (oa_epg_rect){ 1196, 266, 1446, 311 };
    out->time_r = (oa_epg_rect){ 1713, 266, 1868, 311 };
    out->icon_left = (oa_epg_rect){ 620, 265, 674, 319 };
    out->icon_right = (oa_epg_rect){ 1868, 265, 1922, 319 };
    out->list = (oa_epg_rect){ 3, 319, 1920, 1043 };
    out->service_list = (oa_epg_rect){ 0, 319, 640, 1043 };
    for (int i = 0; i < 5; i++)
        out->tabs[i] = (oa_epg_rect){ 206 + i * 302, 198,
                                     506 + i * 302, 257 };
    /* ListTimelineBox interval global 0x178d628 initializes to 7200; zoom
     * steps 3600 s between 3600 and 14400 s. */
    out->interval_s = 7200;

    const uint8_t panel[3] = { 255, 255, 255 };
    const uint8_t header[3] = { 72, 23, 48 };       /* #481730 family header */
    const uint8_t under[3] = { 190, 30, 45 };       /* #BE1E2D at 9/10 */
    /* #010101 at 5/10 over the header. */
    const uint8_t bar[3] = { 36, 11, 24 };
    const uint8_t list[3] = { 197, 201, 205 };      /* #C5C9CD */
    const uint8_t service[3] = { 255, 255, 255 };
    const uint8_t border[3] = { 35, 31, 32 };       /* #231F20 */
    const uint8_t text[3] = { 255, 255, 255 };
    const uint8_t event[3] = { 247, 148, 30 };      /* #F7941E */
    const uint8_t arrow[3] = { 0, 159, 147 };       /* #009F93 */
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->header_rgb, header, sizeof header);
    memcpy(out->under_rgb, under, sizeof under);
    memcpy(out->date_bar_rgb, bar, sizeof bar);
    memcpy(out->list_rgb, list, sizeof list);
    memcpy(out->service_rgb, service, sizeof service);
    memcpy(out->service_border_rgb, border, sizeof border);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->event_title_rgb, event, sizeof event);
    memcpy(out->arrow_rgb, arrow, sizeof arrow);
    return 0;
}

int oa_epg_list_split(const oa_epg_rect *list, int service_width,
                      oa_epg_rect *services, oa_epg_rect *timeline)
{
    if (!list || !services || !timeline || service_width <= 0)
        return -EINVAL;
    int width = list->x1 - list->x;
    if (width <= 0 || list->y1 <= list->y)
        return -EINVAL;
    /* createServiceList 0x00dc9ce4: x1 = list.x + service width + 1. */
    long x1 = (long)list->x + service_width + 1;
    if (x1 > list->x1)
        x1 = list->x1;
    if (x1 <= list->x)
        return -EINVAL;
    *services = (oa_epg_rect){ list->x, list->y, (int)x1, list->y1 };
    *timeline = (oa_epg_rect){ (int)x1, list->y, list->x1, list->y1 };
    return 0;
}

/* Truncating ratio as the shipped multiply/divide helper does. Both operands
 * are non-negative here; the cast keeps 64-bit intermediates. */
static int64_t ratio(int64_t pixels, int64_t seconds, int64_t interval)
{
    return (pixels * seconds) / interval;
}

int oa_epg_event_span(const oa_epg_rect *timeline, int64_t window_start_ms,
                      int32_t interval_s, int64_t start_ms, int64_t end_ms,
                      int *x, int *w)
{
    if (!timeline || !x || !w || interval_s <= 0 || end_ms < start_ms)
        return -EINVAL;
    int width = timeline->x1 - timeline->x;
    if (width <= 0)
        return -EINVAL;
    int64_t window = (int64_t)window_start_ms;
    int64_t end_window = window + (int64_t)interval_s * 1000;
    int64_t clip_start = start_ms > window ? start_ms : window;
    int64_t clip_end = end_ms < end_window ? end_ms : end_window;
    if (clip_start >= clip_end)
        return -ERANGE;
    int64_t off = ratio(width, (clip_start - window) / 1000, interval_s);
    int64_t off_end = ratio(width, (clip_end - window) / 1000, interval_s);
    if (off_end > width)
        off_end = width;
    if (off_end <= off)
        return -ERANGE;
    *x = timeline->x + (int)off;
    *w = (int)(off_end - off);
    return 0;
}

int oa_epg_instant_x(const oa_epg_rect *timeline, int64_t window_start_ms,
                     int32_t interval_s, int64_t at_ms, int *x)
{
    if (!timeline || !x || interval_s <= 0)
        return -EINVAL;
    int width = timeline->x1 - timeline->x;
    if (width <= 0)
        return -EINVAL;
    int64_t delta = at_ms - window_start_ms;
    if (delta < 0 || delta >= (int64_t)interval_s * 1000)
        return -ERANGE;
    *x = timeline->x + (int)ratio(width, delta / 1000, interval_s);
    return 0;
}

int oa_epg_focus_event(const struct oa_epg_event *events, size_t count,
                       int64_t focus_ms)
{
    if (!events || count == 0)
        return -1;
    for (size_t i = 0; i < count; i++) {
        if (events[i].end_ms > focus_ms)
            return (int)i;
    }
    return (int)count - 1;
}

int oa_epg_visible_rows(const oa_epg_layout *layout, int row_pitch)
{
    if (!layout || row_pitch <= 0)
        return -EINVAL;
    int height = layout->list.y1 - layout->list.y;
    if (height <= 0)
        return 0;
    return height / row_pitch;
}

void oa_epg_event_style(int focused, int active,
                        struct oa_epg_event_style *out)
{
    if (!out)
        return;
    /* serviceBFA/UFA/FI/UFI record 456/457/458/459. */
    static const struct oa_epg_event_style fa = {
        { 247, 148, 30 }, { 211, 123, 28 }, { 126, 0, 10 }
    };
    static const struct oa_epg_event_style ufa = {
        { 126, 0, 10 }, { 109, 1, 15 }, { 255, 255, 255 }
    };
    static const struct oa_epg_event_style fi = {
        { 237, 237, 237 }, { 216, 216, 216 }, { 126, 0, 10 }
    };
    if (focused)
        *out = active ? fa : fi;
    else
        *out = ufa;
}

int oa_epg_synopsis_default(oa_epg_synopsis *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_epg_rect){ 0, 0, 1920, 1080 };
    out->header = (oa_epg_rect){ 0, 0, 1920, 264 };
    out->event_bar = (oa_epg_rect){ 0, 264, 1920, 571 };
    out->dialog_title = (oa_epg_rect){ 164, 21, 560, 75 };
    out->channel_name = (oa_epg_rect){ 590, 9, 1434, 81 };
    out->event_title = (oa_epg_rect){ 593, 84, 1437, 144 };
    out->service_logo = (oa_epg_rect){ 204, 43, 354, 193 };
    out->channel_number = (oa_epg_rect){ 358, 65, 538, 137 };
    out->event_time = (oa_epg_rect){ 290, 149, 551, 194 };
    out->clock = (oa_epg_rect){ 1175, 149, 1434, 194 };
    out->event_image = (oa_epg_rect){ 62, 299, 382, 479 };
    out->event_text = (oa_epg_rect){ 429, 273, 1882, 512 };
    out->event_duration = (oa_epg_rect){ 76, 472, 396, 517 };
    out->date = (oa_epg_rect){ 44, 518, 368, 562 };
    out->time_m = (oa_epg_rect){ 1196, 519, 1446, 563 };
    out->icon_right = (oa_epg_rect){ 1866, 516, 1920, 570 };
    out->list = (oa_epg_rect){ 0, 570, 1920, 1077 };
    out->service_list = (oa_epg_rect){ 0, 572, 640, 994 };
    for (int i = 0; i < 5; i++)
        out->tabs[i] = (oa_epg_rect){ 206 + i * 302, 198,
                                     506 + i * 302, 257 };
    out->interval_s = 7200;
    const uint8_t panel[3] = { 255, 255, 255 };
    const uint8_t header[3] = { 72, 23, 48 };
    const uint8_t bar[3] = { 36, 11, 24 };          /* 5/10 over the header */
    const uint8_t list[3] = { 197, 201, 205 };
    /* Record 102's service column is #C5C9CD (not the 101 white). */
    const uint8_t service[3] = { 197, 201, 205 };
    const uint8_t border[3] = { 35, 31, 32 };
    const uint8_t text[3] = { 255, 255, 255 };
    const uint8_t event[3] = { 247, 148, 30 };
    const uint8_t arrow[3] = { 0, 159, 147 };
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->header_rgb, header, sizeof header);
    memcpy(out->event_bar_rgb, bar, sizeof bar);
    memcpy(out->list_rgb, list, sizeof list);
    memcpy(out->service_rgb, service, sizeof service);
    memcpy(out->service_border_rgb, border, sizeof border);
    memcpy(out->text_rgb, text, sizeof text);
    memcpy(out->event_title_rgb, event, sizeof event);
    memcpy(out->arrow_rgb, arrow, sizeof arrow);
    return 0;
}

int oa_epg_tab_at(const oa_epg_layout *layout, int x, int y)
{
    if (!layout)
        return -1;
    for (int i = 0; i < 5; i++) {
        const oa_epg_rect *r = &layout->tabs[i];
        if (x >= r->x && x < r->x1 && y >= r->y && y < r->y1)
            return i;
    }
    return -1;
}
