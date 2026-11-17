#ifndef OPENAURORA_EPG_LAYOUT_H
#define OPENAURORA_EPG_LAYOUT_H

#include <stddef.h>
#include <stdint.h>

#include "openaurora/broadcast.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Stock EPG timeline layout recovered from DialogEPG.svg (BinSVG 101) and the
 * MultiServiceTimelineScheduleDialog / ListTimelineBox / ServiceTimelineBox
 * bodies (aurora MD5 64c252340d3af6c5d66072611b7bad7a):
 *   dialog 0,0,1920,1080 white; red under-list band 0,319,1920,1080 #BE1E2D
 *   at 9/10; date bar 0,264,1920,318 #010101 at 5/10; list 3,319,1920,1043
 *   #C5C9CD; service column 0,319,640,1043 white with #231F20; five tab
 *   buttons 206+302*i,198,506+302*i,257.
 * createServiceList 0x00dc9ce4 splits the list at
 *   service.x1 = list.x + EPGServiceListBox.width + 1
 * so the timeline is 644,319,1920,1043 for this resource.
 *
 * ServiceTimelineBox::createButton 0x00dd4914 computes an event bar as
 *   x  = boxWidth * (eventStart - windowStart) / interval
 *   x1 = boxWidth * (eventEnd   - windowStart) / interval   (clamped)
 * with the window start/end at +0x78/+0x7c, the interval in seconds at +0x80
 * and the box width at +0x68. ListTimelineBox::zoom 0x00dc6f1c steps that
 * interval by 3600 s between 3600 and 14400 s; the initialized default is
 * 7200 s (2 h). DateTime::difference returns seconds.
 *
 * updateTimelineTitleBar 0x00dc9950 sets the date from
 * TimeDateServices::getDateString and the three labels from
 * TimeDateServices::getTimeString: window start, middle, end. TimeDateServices
 * formats "<weekday-short> %02u/%02u/%u" (with the year) and "%02u:%02u".
 *
 * The row pitch is NOT part of this model: SVGListBoxBase::rowHeight comes
 * from the scaled row art at runtime and is not pinned offline. Callers pass
 * an explicit pitch (see oa_epg_visible_rows); do not fit one from a capture.
 * This helper performs no UI, SVG or host I/O.
 */
typedef struct {
    int x, y, x1, y1;
} oa_epg_rect;

typedef struct {
    oa_epg_rect panel;          /* 0,0,1920,1080 */
    oa_epg_rect header;         /* 0,0,1920,319 family header, see below */
    oa_epg_rect under_list;     /* 0,319,1920,1080 red band */
    oa_epg_rect date_bar;       /* 0,264,1920,318 */
    oa_epg_rect dialog_title;   /* 164,21,560,75 */
    oa_epg_rect channel_name;   /* 590,9,1434,81 */
    oa_epg_rect event_title;    /* 593,84,1437,144 */
    oa_epg_rect service_logo;   /* 204,43,354,193 */
    oa_epg_rect channel_number; /* 358,65,538,137 */
    oa_epg_rect event_time;     /* 290,149,551,194 */
    oa_epg_rect clock;          /* 1175,149,1434,194 */
    oa_epg_rect date;           /* 44,266,368,311 */
    oa_epg_rect time_l;         /* 677,266,927,311 */
    oa_epg_rect time_m;         /* 1196,266,1446,311 */
    oa_epg_rect time_r;         /* 1713,266,1868,311 */
    oa_epg_rect icon_left;      /* 620,265,674,319 */
    oa_epg_rect icon_right;     /* 1868,265,1922,319 */
    oa_epg_rect list;           /* 3,319,1920,1043 */
    oa_epg_rect service_list;   /* 0,319,640,1043 */
    oa_epg_rect tabs[5];        /* 206+302*i,198,506+302*i,257 */
    int interval_s;             /* recovered default 7200 */
    /* header_rgb is the full-screen dialog family header (DialogPlayList
     * resource #481730); record 101 leaves the top band on its white canvas and
     * the runtime theme supplies the header, so this is a family inference, not
     * a record-101 fill. date_bar_rgb is #010101 at 5/10 composed over it. */
    uint8_t panel_rgb[3], header_rgb[3], under_rgb[3], date_bar_rgb[3];
    uint8_t list_rgb[3], service_rgb[3], service_border_rgb[3];
    uint8_t text_rgb[3], event_title_rgb[3], arrow_rgb[3];
} oa_epg_layout;

/* Fill the recovered defaults. -EINVAL for NULL. */
int oa_epg_layout_default(oa_epg_layout *out);

/* Split the list at service.x1 = list.x + service_width + 1, clamped to the
 * list. `services` and `timeline` may not be NULL; both get exclusive edges.
 * Returns 0 or -EINVAL (NULL, empty list, nonpositive width). */
int oa_epg_list_split(const oa_epg_rect *list, int service_width,
                      oa_epg_rect *services, oa_epg_rect *timeline);

/* Event bar inside `timeline` for a window of `interval_s` starting at
 * window_start_ms. Returns 0 and a positive width when the event overlaps the
 * window, -ERANGE when it does not (or the clipped width is 0) and -EINVAL for
 * bad arguments (NULL, interval<=0, width<=0, end<start). The vendor computes
 * with truncating division; x/w are absolute screen pixels. */
int oa_epg_event_span(const oa_epg_rect *timeline, int64_t window_start_ms,
                      int32_t interval_s, int64_t start_ms, int64_t end_ms,
                      int *x, int *w);

/* x of an instant (the now indicator) using the same ratio; -ERANGE when the
 * instant is outside [window_start, window_start+interval). */
int oa_epg_instant_x(const oa_epg_rect *timeline, int64_t window_start_ms,
                     int32_t interval_s, int64_t at_ms, int *x);

/* Rows whose top starts inside the list for an explicit row pitch. Returns
 * -EINVAL for NULL/nonpositive pitch, else the count. The caller clips the
 * last partial row; no pitch is implied by this helper. */
int oa_epg_visible_rows(const oa_epg_layout *layout, int row_pitch);

/* 0..4 tab index, -1 outside (exclusive edges). */
int oa_epg_tab_at(const oa_epg_layout *layout, int x, int y);

/*
 * Focused event index, from ServiceTimelineBox::setFocusedEvent 0x00dd4c5c:
 * walk the event list in order and take the first event whose end is after
 * focus_ms; if none ends after it, take the last event. Returns -1 for an empty
 * list or a NULL array. `events` must be in start order (the vendor list order).
 * The caller chooses focus_ms: the vendor passes the last focused event time,
 * or the current time; setFocusTime falls back to the window middle when the
 * current time is outside the window.
 */
int oa_epg_focus_event(const struct oa_epg_event *events, size_t count,
                       int64_t focus_ms);

/*
 * Event-button state colours from serviceB{FA,UFA,FI,UFI}.svg (BinSVG
 * 456/457/458/459; SVGServiceEventButton::resetSVGNames 0x00dd5404 selects
 * 456/457/458/459 for the default path and 217..224 for OnDemand). The row
 * fill, the lower time strip fill and the text ink are distinct:
 *   FA  row #F7941E time #D37B1C text #7E000A
 *   UFA row #7E000A time #6D010F text #FFFFFF
 *   FI  row #EDEDED time #D8D8D8 text #7E000A
 *   UFI row #7E000A time #6D010F text #FFFFFF
 * `focused` selects FA/FI and `active` (start <= now < end) selects A/I.
 * Unfocused rows share one fill, so A/I only changes the focused colour.
 */
struct oa_epg_event_style {
    uint8_t fill[3];
    uint8_t time_fill[3];
    uint8_t text[3];
};
void oa_epg_event_style(int focused, int active,
                        struct oa_epg_event_style *out);

/*
 * Synopsis view (DialogEPG_synopsis.svg, BinSVG 102), selected by
 * MultiServiceTimelineScheduleDialog::showSynopsis 0x00dca1b0
 * (SVGDialog::setSVGName 0x66) and hidden by hideSynopsis (0x65). The top
 * 0..264 is the family header again; 0,264,1920,571 is #010101 at 5/10;
 * event image 62,299,382,479; event text 429,273,1882,512; duration
 * 76,472,396,517; list 0,570,1920,1077 with the service column
 * 0,572,640,994. Tab rectangles are unchanged.
 */
typedef struct {
    oa_epg_rect panel;          /* 0,0,1920,1080 */
    oa_epg_rect header;         /* 0,0,1920,264 family inference */
    oa_epg_rect event_bar;      /* 0,264,1920,571 */
    oa_epg_rect dialog_title;   /* 164,21,560,75 */
    oa_epg_rect channel_name;   /* 590,9,1434,81 */
    oa_epg_rect event_title;    /* 593,84,1437,144 */
    oa_epg_rect service_logo;   /* 204,43,354,193 */
    oa_epg_rect channel_number; /* 358,65,538,137 */
    oa_epg_rect event_time;     /* 290,149,551,194 */
    oa_epg_rect clock;          /* 1175,149,1434,194 */
    oa_epg_rect event_image;    /* 62,299,382,479 */
    oa_epg_rect event_text;     /* 429,273,1882,512 */
    oa_epg_rect event_duration; /* 76,472,396,517 */
    oa_epg_rect date;           /* 44,518,368,562 */
    oa_epg_rect time_m;         /* 1196,519,1446,563 */
    oa_epg_rect icon_right;     /* 1866,516,1920,570 */
    oa_epg_rect list;           /* 0,570,1920,1077 */
    oa_epg_rect service_list;   /* 0,572,640,994 */
    oa_epg_rect tabs[5];
    int interval_s;
    uint8_t panel_rgb[3], header_rgb[3], event_bar_rgb[3];
    uint8_t list_rgb[3], service_rgb[3], service_border_rgb[3];
    uint8_t text_rgb[3], event_title_rgb[3], arrow_rgb[3];
} oa_epg_synopsis;

/* Fill the record-102 defaults. -EINVAL for NULL. */
int oa_epg_synopsis_default(oa_epg_synopsis *out);

#ifdef __cplusplus
}
#endif

#endif
