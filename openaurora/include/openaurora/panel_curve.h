/* Vendor backlight curve recovered from DEV_PANEL_SetBacklightPWMCurve and
 * DEV_PANEL_SetPWMBacklightLv.
 *
 * DEV_PANEL_Init calls SetBacklightPWMCurve(40,60,80,100) and sets the level
 * scale to 1000. SetPWMBacklightLimit writes the runtime minimum
 * gstBacklightPara+4; SetPWMBacklightLv maps level 0..1000 through that table.
 * All arithmetic below uses the vendor's integer rounding (+50/100, +500/1000).
 */
#ifndef OPENAURORA_PANEL_CURVE_H
#define OPENAURORA_PANEL_CURVE_H
#include <stdint.h>

typedef struct {
    uint16_t min;   /* gstBacklightPara+4, 0..1000 */
    uint16_t p[4];  /* breakpoint inputs, vendor default 40,60,80,100 */
    uint16_t v[3];  /* mapped v(p0..2) */
    uint16_t mid;   /* gstVoutPwmParams+16 */
} oa_panel_curve;

/* Build the vendor table from four percentage breakpoints and a raw minimum
 * (0..1000). Inputs are clamped to 0..100 / 0..1000. */
void oa_panel_curve_init(oa_panel_curve *c, int p1, int p2, int p3, int p4,
                         int min_raw);

/* Map an integer level 0..1000 through the curve. Returns 0..1000. If a
 * degenerate table would divide by zero, returns the pre-curve value instead of
 * taking the vendor divider fault. */
int oa_panel_curve_map(const oa_panel_curve *c, int level);

/* UI percentage 0..100 -> level 0..1000 -> curve -> raw duty 0..duty_max. */
int oa_panel_percent_to_duty(const oa_panel_curve *c, int percent, int duty_max);

#endif
