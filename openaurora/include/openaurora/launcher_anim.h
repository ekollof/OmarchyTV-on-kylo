/* Recovered sub-launcher grid and SVGButton animation geometry.
 *
 * Sources:
 *   SVGGrid::renderBackground 0x010c9ed0:
 *     if (row_pitch > 0) rows = (area_h - y_off - (button_h - row_pitch)) / row_pitch;
 *     if (col_pitch > 0) cols = (area_w - x_off - (button_w - col_pitch)) / col_pitch;
 *     fields +0x1dc/+0x1e0 are the resulting counts. The stored +0x1f8/+0x200
 *     values are button_h-row_pitch and button_w-col_pitch.
 *   SVGGrid::getButtonPosition 0x010ca8ac:
 *     x = col*col_pitch + x_off; y = row*row_pitch + y_off;
 *     right = x + col_pitch + (button_w-col_pitch); bottom = y + button_h.
 *   SVGButton::animRectDecode 0x010b0854:
 *     elapsed_ms / (duration_seconds*1000), clamped above to 1;
 *     scale = progress*to + (1-progress)*from;
 *     scaled rect origin is the button window top-left, size is the named
 *     scalerect width/height times scale, float-to-int truncation.
 *   AnimationHelper::applyMotionAnimation 0x00eefd20:
 *     offset = progress*to + (1-progress)*from, added to the base point and
 *     truncated to int.
 * These are pure geometry rules. No screen/layout policy is implied; the
 * category rail has no animation records and must not use them.
 */
#ifndef OPENAURORA_LAUNCHER_ANIM_H
#define OPENAURORA_LAUNCHER_ANIM_H

typedef struct {
    int x1, y1, x2, y2;
} oa_launcher_rect;

void oa_launcher_grid_counts(int area_w, int area_h, int x_off, int y_off,
                             int col_pitch, int row_pitch, int button_w,
                             int button_h, int *cols, int *rows);

/* window_x/window_y is the button window origin, not the named rectangle's
 * stored x/y. width/height come from the #scalerect named rectangle. */
void oa_launcher_scale_rect(int window_x, int window_y, int scalerect_w,
                            int scalerect_h, float from, float to,
                            float progress, oa_launcher_rect *out);

void oa_launcher_motion_point(int x, int y, int from_x, int from_y, int to_x,
                              int to_y, float progress, int *out_x,
                              int *out_y);

#endif
