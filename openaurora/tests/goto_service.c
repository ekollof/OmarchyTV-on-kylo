/* Channel GotoServiceDialog: record-116 layout and numeric entry/lookup. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "openaurora/goto_service.h"

int main(void)
{
    oa_goto_layout l;
    assert(oa_goto_layout_default(&l) == 0);
    assert(l.panel.x == 0 && l.panel.y == 0 && l.panel.x1 == 540 &&
           l.panel.y1 == 239);
    assert(l.number_box.x == -133 && l.number_box.y == 98 &&
           l.number_box.x1 == 377 && l.number_box.y1 == 298);
    assert(l.animation.x == 0 && l.animation.y == 308 &&
           l.animation.x1 == 100 && l.animation.y1 == 408);
    assert(l.entry_box.x == 41 && l.entry_box.y == 45 &&
           l.entry_box.x1 == 209 && l.entry_box.y1 == 212);
    assert(l.font_id == 15 && l.entry_mode == 11 && l.timeout_ms == 1500 &&
           l.timeout_shortcut_ms == 3000);
    assert(l.panel_rgb[0] == 255 && l.number_rgb[0] == 35);
    assert(l.entry_rgb[0] == 72 && l.entry_rgb[1] == 23 &&
           l.entry_rgb[2] == 48);
    assert(l.digit_rgb[0] == 255 && l.digit_rgb[1] == 255 &&
           l.digit_rgb[2] == 255);
    /* Show animation: record 116 has animation_slide_down_fade_in (probe id
     * 0x3d -> type 5), 1000 ms, 150 ms controller tick. */
    assert(l.anim.type == OA_GOTO_ANIM_SLIDE_DOWN);
    assert(l.anim.duration_ms == 1000 && l.anim.tick_ms == 150);
    assert(OA_GOTO_ANIM_FLIP == 1 && OA_GOTO_ANIM_BOUNCE == 8);
    assert(oa_goto_layout_default(NULL) == -EINVAL);

    oa_goto_entry e;
    oa_goto_reset(&e, 0);
    assert(e.len == 0 && e.max_digits == OA_GOTO_DEFAULT_MAX_DIGITS);
    assert(oa_goto_value(&e) == 0);
    assert(oa_goto_backspace(&e) == -ENODATA);
    assert(oa_goto_digit(&e, 7) == 0);
    assert(oa_goto_digit(&e, 9) == 0);
    assert(!strcmp(e.digits, "79") && oa_goto_value(&e) == 79);
    assert(oa_goto_digit(&e, -1) == -EINVAL);
    assert(oa_goto_digit(&e, 10) == -EINVAL);
    assert(oa_goto_digit(NULL, 1) == -EINVAL);
    assert(oa_goto_backspace(&e) == 0);
    assert(!strcmp(e.digits, "7") && oa_goto_value(&e) == 7);

    /* Leading zeros keep walking every digit (getNumericContents). */
    oa_goto_reset(&e, 4);
    assert(oa_goto_digit(&e, 0) == 0);
    assert(oa_goto_digit(&e, 0) == 0);
    assert(oa_goto_digit(&e, 7) == 0);
    assert(!strcmp(e.digits, "007") && oa_goto_value(&e) == 7);
    assert(oa_goto_digit(&e, 1) == 0);
    assert(oa_goto_digit(&e, 2) == -ENOSPC);      /* 4-digit bound */

    /* Seven digits stay exact; the value helper saturates before wrapping
     * (an 8th digit is outside the entry buffer). */
    oa_goto_reset(&e, 7);
    for (int i = 0; i < 7; i++)
        assert(oa_goto_digit(&e, 9) == 0);
    assert(oa_goto_value(&e) == 9999999u);
    oa_goto_reset(NULL, 4);                      /* explicit no-op */
    assert(oa_goto_backspace(NULL) == -EINVAL);
    assert(oa_goto_value(NULL) == 0);

    static const uint32_t numbers[] = { 7, 9, 5 };
    assert(oa_goto_find(numbers, 3, 9) == 1);
    assert(oa_goto_find(numbers, 3, 5) == 2);
    assert(oa_goto_find(numbers, 3, 42) == -ENOENT);
    assert(oa_goto_find(NULL, 0, 1) == -ENOENT);
    assert(oa_goto_find(NULL, 1, 1) == -EINVAL);

    puts("ok goto service: record-116 layout, digits, value and lookup");
    return 0;
}
