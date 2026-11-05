/* Pure recovered SVGGrid/SVGButton animation geometry. No display or device. */
#include <assert.h>
#include <stdio.h>
#include "openaurora/launcher_anim.h"

int main(void)
{
    int cols = -1, rows = -1;
    /* (area - offset - (button - pitch)) / pitch, ARM signed division. */
    oa_launcher_grid_counts(1000, 500, 10, 20, 100, 120, 90, 110, &cols, &rows);
    assert(cols == 10 && rows == 4);
    oa_launcher_grid_counts(1585, 222, 0, 0, 250, 160, 240, 140, &cols, &rows);
    assert(cols == 6 && rows == 1);
    oa_launcher_grid_counts(10, 10, 20, 20, 0, 0, 100, 100, &cols, &rows);
    assert(cols == 6 && rows == 1); /* zero pitches do not write */

    oa_launcher_rect r;
    /* sublauncheritemFA #scalerect 240x140, 1 -> 1.35, window origin 100,200. */
    oa_launcher_scale_rect(100, 200, 240, 140, 1.0f, 1.35f, 0.0f, &r);
    assert(r.x1 == 100 && r.y1 == 200 && r.x2 == 340 && r.y2 == 340);
    oa_launcher_scale_rect(100, 200, 240, 140, 1.0f, 1.35f, 0.5f, &r);
    assert(r.x1 == 100 && r.y1 == 200 && r.x2 == 382 && r.y2 == 364);
    oa_launcher_scale_rect(100, 200, 240, 140, 1.0f, 1.35f, 1.0f, &r);
    assert(r.x1 == 100 && r.y1 == 200 && r.x2 == 424 && r.y2 == 389);

    int x = 0, y = 0;
    oa_launcher_motion_point(50, 60, 0, 0, -42, -24, 0.0f, &x, &y);
    assert(x == 50 && y == 60);
    oa_launcher_motion_point(50, 60, 0, 0, -42, -24, 0.5f, &x, &y);
    assert(x == 29 && y == 48);
    oa_launcher_motion_point(50, 60, 0, 0, -42, -24, 1.0f, &x, &y);
    assert(x == 8 && y == 36);

    /* 300 ms controller: elapsed/duration, capped at 1. */
    assert(150.0f / 300.0f == 0.5f);
    puts("ok launcher animation: grid counts, top-left scalerect and linear glyph motion");
    return 0;
}
