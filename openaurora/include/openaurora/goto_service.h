#ifndef OPENAURORA_GOTO_SERVICE_H
#define OPENAURORA_GOTO_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Channel GotoServiceDialog, recovered from GotoServiceDialog and
 * AdvancedChannelListDialog::onDigitPress (aurora MD5
 * 64c252340d3af6c5d66072611b7bad7a):
 *
 * - Dialog name "GotoService"; layout from DialogGoToService.svg (BinSVG 116):
 *   background 0,0,1918,1080 white; main_rectangle 0,0,540,239 white;
 *   serviceNumber -133,98,377,298 (the authored pre-transform text box);
 *   animation_slide_down_fade_in 0,308,100,408.
 * - init 0x00d2296c looks up "dash", "bigFont" and "serviceNumber", sets the
 *   text entry mode to 0xb (11), font 0xf (15), maps the entry states to
 *   resources 0x18b..0x18e (BinSVG 395..398: gotoServiceTextEntryBoxFA/FI/
 *   UFA/UFI) and creates the box from the serviceNumber rect.
 * - onDigit 0x00d2262c forwards the key and restarts a 1500 ms timer (3000 ms
 *   while the keys-shortcut dialog is open); handleMessage 0x00d226c8 maps
 *   digits 0x200..0x209 to onDigit, Select 0x20e and timer 0x1006/1 to
 *   findServiceAndEndDialog, and 0x21f/0x28b to onStandby.
 * - findServiceAndEndDialog 0x00d223f8 reads getNumericContents and looks the
 *   number up in the service list, ending the modal with the resulting item
 *   index; onDigitPress 0x00e7cb18 then calls setSelectedItem(result).
 *
 * onDigitPress first calls totalNumberOfItems() @0x00e7cb40 and skips the
 * modal when the list is empty (cbz @0x00e7cb44).
 *
 * This model implements the numeric entry, lookup and result semantics. A
 * read-only on-TV fb1 sample (2026-11-19) pinned the live render: the dialog
 * is static for ~1500 ms and paints only a flat #481730 167x167 entry box at
 * (41,45)..(207,211) with white centred digit ink, over the still-visible
 * channel screen — the named main_rectangle is layout metadata, not paint.
 * The authored serviceNumber rect is pre-transform; the transform to the
 * rendered box is still unrecovered. The max digit count is not reduced from
 * the create arguments (12/13), so callers pass an explicit bound (the screen
 * uses ui.goto_max_digits, default 4).
 */
typedef struct {
    int x, y, x1, y1;
} oa_goto_rect;

typedef struct {
    oa_goto_rect panel;       /* 0,0,540,239 authored (layout, not paint) */
    oa_goto_rect number_box;  /* -133,98,377,298 authored (pre-transform) */
    oa_goto_rect animation;   /* 0,308,100,408 */
    /* Live stock render, read-only fb1 sampler 2026-11-19: the dialog paints
     * a flat 167x167 #481730 entry box at 41,45..207,211 with the white digit
     * ink centred at 125,127. The named main_rectangle/serviceNumber records
     * are layout metadata, not opaque paint (as for DialogMainMenu). */
    oa_goto_rect entry_box;   /* 41,45,207,211 rendered */
    int font_id;              /* 15 */
    int entry_mode;           /* 11 */
    int timeout_ms;           /* 1500 */
    int timeout_shortcut_ms;  /* 3000 */
    unsigned char panel_rgb[3];
    unsigned char number_rgb[3];
    unsigned char entry_rgb[3];  /* 72,23,48 = #481730 */
    unsigned char digit_rgb[3];  /* 255,255,255 */
} oa_goto_layout;

typedef struct {
    char digits[8];
    unsigned len;
    unsigned max_digits;
} oa_goto_entry;

#define OA_GOTO_DEFAULT_MAX_DIGITS 4u

/* Fill the record-116 defaults. -EINVAL for NULL. */
int oa_goto_layout_default(oa_goto_layout *out);

/* Reset to empty with max_digits (0 selects the default, capped at 7). */
void oa_goto_reset(oa_goto_entry *entry, unsigned max_digits);

/* Append a decimal digit. 0 on success, -EINVAL for a non-digit or NULL,
 * -ENOSPC when the entry is full. */
int oa_goto_digit(oa_goto_entry *entry, int digit);

/* Remove the last digit. 0 on success, -ENODATA when empty, -EINVAL NULL. */
int oa_goto_backspace(oa_goto_entry *entry);

/* Parsed value, capped at UINT32_MAX; 0 for NULL/empty. Leading zeros are
 * accepted (the vendor getNumericContents walks every digit). */
uint32_t oa_goto_value(const oa_goto_entry *entry);

/* First index whose number equals value, -ENOENT when absent and -EINVAL for
 * NULL with a nonzero count. */
int oa_goto_find(const uint32_t *numbers, size_t count, uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
