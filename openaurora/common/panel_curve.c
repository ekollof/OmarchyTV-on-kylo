#include "openaurora/panel_curve.h"

static int clamp(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void oa_panel_curve_init(oa_panel_curve *c, int p1, int p2, int p3, int p4,
                         int min_raw)
{
    if (!c) return;
    c->min = (uint16_t)clamp(min_raw, 0, 1000);
    c->p[0] = (uint16_t)clamp(p1, 0, 100);
    c->p[1] = (uint16_t)clamp(p2, 0, 100);
    c->p[2] = (uint16_t)clamp(p3, 0, 100);
    c->p[3] = (uint16_t)clamp(p4, 0, 100);
    int span = 1000 - c->min;
    for (int i = 0; i < 3; i++)
        c->v[i] = (uint16_t)(c->min + (span * c->p[i] + 50) / 100);
    int u = (c->v[1] * c->p[3] + 50) / 100;
    int lo = (c->v[0] + c->v[1]) / 2;
    int hi = (c->v[2] + c->v[1]) / 2;
    if (u < lo) c->mid = (uint16_t)lo;
    else if (u <= hi) c->mid = (uint16_t)u;
    else c->mid = (uint16_t)hi;
}

int oa_panel_curve_map(const oa_panel_curve *c, int level)
{
    if (!c) return 0;
    level = clamp(level, 0, 1000);
    int span = 1000 - c->min;
    int x = c->min + (level * span + 500) / 1000;
    if (x <= c->v[0] || x >= c->v[2]) return x;

    int a, b, den, num;
    if (x <= c->v[1]) {
        a = c->v[0];
        b = c->mid;
        den = c->v[1] - c->v[0];
        num = x - c->v[0];
    } else {
        a = c->mid;
        b = c->v[2];
        den = c->v[2] - c->v[1];
        num = x - c->v[1];
    }
    if (den <= 0) return x; /* vendor divider would fault; stay defensive */
    int slope = (1000 * (b - a)) / den;
    int out = a + (num * slope) / 1000;
    return clamp(out, 0, 1000);
}

int oa_panel_percent_to_duty(const oa_panel_curve *c, int percent, int duty_max)
{
    if (duty_max <= 0) return 0;
    percent = clamp(percent, 0, 100);
    int level = oa_panel_curve_map(c, percent * 10);
    return clamp(level * duty_max / 1000, 0, duty_max);
}
