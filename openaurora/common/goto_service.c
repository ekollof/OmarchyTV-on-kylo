/* Channel GotoServiceDialog model: record-116 layout and numeric entry. Pure. */
#include "openaurora/goto_service.h"

#include <errno.h>
#include <string.h>

int oa_goto_layout_default(oa_goto_layout *out)
{
    if (!out)
        return -EINVAL;
    memset(out, 0, sizeof *out);
    out->panel = (oa_goto_rect){ 0, 0, 540, 239 };
    out->number_box = (oa_goto_rect){ -133, 98, 377, 298 };
    out->animation = (oa_goto_rect){ 0, 308, 100, 408 };
    out->entry_box = (oa_goto_rect){ 41, 45, 208, 212 };
    out->font_id = 15;
    out->entry_mode = 11;
    out->timeout_ms = 1500;
    out->timeout_shortcut_ms = 3000;
    const unsigned char panel[3] = { 255, 255, 255 };
    /* Authored ink (used by other dialogs) plus the measured live-render
     * entry box: flat #481730 with white digit ink. See the header. */
    const unsigned char number[3] = { 35, 31, 32 };
    const unsigned char entry[3] = { 72, 23, 48 };
    const unsigned char digit[3] = { 255, 255, 255 };
    memcpy(out->panel_rgb, panel, sizeof panel);
    memcpy(out->number_rgb, number, sizeof number);
    memcpy(out->entry_rgb, entry, sizeof entry);
    memcpy(out->digit_rgb, digit, sizeof digit);
    return 0;
}

void oa_goto_reset(oa_goto_entry *entry, unsigned max_digits)
{
    if (!entry)
        return;
    memset(entry, 0, sizeof *entry);
    if (max_digits == 0)
        max_digits = OA_GOTO_DEFAULT_MAX_DIGITS;
    if (max_digits > sizeof entry->digits - 1)
        max_digits = sizeof entry->digits - 1;
    entry->max_digits = max_digits;
}

int oa_goto_digit(oa_goto_entry *entry, int digit)
{
    if (!entry || digit < 0 || digit > 9)
        return -EINVAL;
    if (entry->len >= entry->max_digits ||
        entry->len >= sizeof entry->digits - 1)
        return -ENOSPC;
    entry->digits[entry->len++] = (char)('0' + digit);
    entry->digits[entry->len] = '\0';
    return 0;
}

int oa_goto_backspace(oa_goto_entry *entry)
{
    if (!entry)
        return -EINVAL;
    if (entry->len == 0)
        return -ENODATA;
    entry->digits[--entry->len] = '\0';
    return 0;
}

uint32_t oa_goto_value(const oa_goto_entry *entry)
{
    if (!entry)
        return 0;
    uint32_t value = 0;
    for (unsigned i = 0; i < entry->len; i++) {
        uint32_t d = (uint32_t)(entry->digits[i] - '0');
        if (value > (UINT32_MAX - d) / 10u)
            return UINT32_MAX;
        value = value * 10u + d;
    }
    return value;
}

int oa_goto_find(const uint32_t *numbers, size_t count, uint32_t value)
{
    if (!numbers)
        return count == 0 ? -ENOENT : -EINVAL;
    for (size_t i = 0; i < count; i++) {
        if (numbers[i] == value)
            return (int)i;
    }
    return -ENOENT;
}
