/* Vendor backlight curve vectors from DEV_PANEL_Init /
 * DEV_PANEL_SetBacklightPWMCurve / DEV_PANEL_SetPWMBacklightLv. */
#include <assert.h>
#include <stdio.h>
#include "openaurora/panel_curve.h"

static void identity(int min_raw)
{
    oa_panel_curve c;
    oa_panel_curve_init(&c, 40, 60, 80, 100, min_raw);
    for (int level = 0; level <= 1000; level += 50)
        assert(oa_panel_curve_map(&c, level) == level);
}

int main(void)
{
    /* Default vendor min 0: the table is the identity. */
    identity(0);

    /* p4=50 moves the middle point to (v1+v2)/2, bending the lower half. */
    oa_panel_curve c;
    oa_panel_curve_init(&c, 40, 60, 80, 50, 0);
    assert(c.v[0] == 400 && c.v[1] == 600 && c.v[2] == 800 && c.mid == 500);
    assert(oa_panel_curve_map(&c, 0) == 0);
    assert(oa_panel_curve_map(&c, 400) == 400);
    assert(oa_panel_curve_map(&c, 500) == 450);
    assert(oa_panel_curve_map(&c, 600) == 500);
    assert(oa_panel_curve_map(&c, 700) == 650);
    assert(oa_panel_curve_map(&c, 800) == 800);
    assert(oa_panel_curve_map(&c, 1000) == 1000);

    /* Runtime minimum 20%: level 0 maps to 200, 1000 to 1000. */
    oa_panel_curve_init(&c, 40, 60, 80, 100, 200);
    assert(oa_panel_curve_map(&c, 0) == 200);
    assert(oa_panel_curve_map(&c, 250) == 400);
    assert(oa_panel_curve_map(&c, 500) == 600);
    assert(oa_panel_curve_map(&c, 750) == 800);
    assert(oa_panel_curve_map(&c, 1000) == 1000);

    /* Percent -> level10 -> curve -> 12-bit PWM duty. */
    oa_panel_curve_init(&c, 40, 60, 80, 100, 0);
    assert(oa_panel_percent_to_duty(&c, 0, 4095) == 0);
    assert(oa_panel_percent_to_duty(&c, 50, 4095) == 2047);
    assert(oa_panel_percent_to_duty(&c, 100, 4095) == 4095);
    assert(oa_panel_percent_to_duty(&c, -5, 4095) == 0);
    assert(oa_panel_percent_to_duty(&c, 150, 4095) == 4095);
    oa_panel_curve_init(&c, 40, 60, 80, 100, 200);
    assert(oa_panel_percent_to_duty(&c, 0, 4095) == 819);
    assert(oa_panel_percent_to_duty(&c, 100, 4095) == 4095);

    /* Degenerate table must not divide by zero (vendor divider would fault). */
    oa_panel_curve_init(&c, 50, 50, 50, 100, 0);
    assert(oa_panel_curve_map(&c, 500) == 500);

    puts("ok panel curve: vendor breakpoints, runtime minimum, piecewise midpoint, percent-duty and bounds");
    return 0;
}
