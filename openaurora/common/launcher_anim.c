#include <limits.h>
#include "openaurora/launcher_anim.h"

static int trunc_int(float value)
{
    if (value >= (float)INT_MAX) return INT_MAX;
    if (value <= (float)INT_MIN) return INT_MIN;
    return (int)value;
}

void oa_launcher_grid_counts(int area_w, int area_h, int x_off, int y_off,
                             int col_pitch, int row_pitch, int button_w,
                             int button_h, int *cols, int *rows)
{
    if (row_pitch > 0 && rows)
        *rows = (area_h - y_off - (button_h - row_pitch)) / row_pitch;
    if (col_pitch > 0 && cols)
        *cols = (area_w - x_off - (button_w - col_pitch)) / col_pitch;
}

void oa_launcher_scale_rect(int window_x, int window_y, int scalerect_w,
                            int scalerect_h, float from, float to,
                            float progress, oa_launcher_rect *out)
{
    if (!out) return;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    float scale = from + (to - from) * progress;
    int width = trunc_int((float)scalerect_w * scale);
    int height = trunc_int((float)scalerect_h * scale);
    out->x1 = window_x;
    out->y1 = window_y;
    out->x2 = window_x + width;
    out->y2 = window_y + height;
}

void oa_launcher_motion_point(int x, int y, int from_x, int from_y, int to_x,
                              int to_y, float progress, int *out_x,
                              int *out_y)
{
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    float dx = (float)from_x + ((float)to_x - (float)from_x) * progress;
    float dy = (float)from_y + ((float)to_y - (float)from_y) * progress;
    if (out_x) *out_x = trunc_int((float)x + dx);
    if (out_y) *out_y = trunc_int((float)y + dy);
}
