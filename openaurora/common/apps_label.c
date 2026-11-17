/* Apps-grid label centering with the recovered physical-width formula.
 * No display or host I/O. */
#include "openaurora/apps_label.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "openaurora/canvas.h"
#include "openaurora/font_text.h"

#define APPS_LABEL_MIN_MARGIN 0
#define APPS_LABEL_MAX_MARGIN 64
#define APPS_LABEL_MAX_HEIGHT 128

int oa_apps_label_render(const struct oa_bitmap_font *font, const char *text,
                         int cell_w, int margin, unsigned char **rgba,
                         int *x, int *w, int *h)
{
    if (!font || !text || !rgba || !x || !w || !h)
        return -EINVAL;
    *rgba = NULL;
    if (cell_w <= 0 || margin < APPS_LABEL_MIN_MARGIN ||
        margin > APPS_LABEL_MAX_MARGIN)
        return -EINVAL;

    int advance = 0, font_height = 0;
    if (oa_font_text_measure(font, text, 0, &advance) != 0 ||
        advance < 1 || advance > cell_w - 16 ||
        oa_bitmap_font_height(font, &font_height) != 0 ||
        font_height < 1 || font_height > APPS_LABEL_MAX_HEIGHT)
        return -ERANGE;

    int width = cell_w, height = font_height + 2 * margin;
    if (width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / 4u / (size_t)height)
        return -EOVERFLOW;
    unsigned char *buf = calloc((size_t)width * (size_t)height * 4u, 1);
    if (!buf)
        return -ENOMEM;
    oa_canvas canvas = { buf, width, height };
    static const uint8_t ink[4] = { 38, 23, 25, 255 };
    /* alignment 3 is the firmware centre path: physical width plus the
     * first-bearing adjustment, with symmetric trimming on overhang. */
    const struct oa_font_line_style style = {
        .x = 0, .y = margin, .left = 0, .right = cell_w, .spacing = 0,
        .alignment = 3
    };
    int rc = oa_font_text_draw_line(&canvas, font, text, &style, ink, NULL,
                                    NULL);
    if (rc != 0) {
        free(buf);
        return rc;
    }

    int minx = width, miny = height, maxx = -1, maxy = -1;
    for (int py = 0; py < height; py++)
        for (int px = 0; px < width; px++)
            if (buf[((size_t)py * (size_t)width + (size_t)px) * 4u + 3u]) {
                if (px < minx) minx = px;
                if (px > maxx) maxx = px;
                if (py < miny) miny = py;
                if (py > maxy) maxy = py;
            }
    if (maxx < minx || maxy < miny) {
        free(buf);
        return -ENOENT;
    }
    int cw = maxx - minx + 1, ch = maxy - miny + 1;
    unsigned char *crop = calloc((size_t)cw * (size_t)ch * 4u, 1);
    if (!crop) {
        free(buf);
        return -ENOMEM;
    }
    for (int py = 0; py < ch; py++)
        memcpy(crop + (size_t)py * (size_t)cw * 4u,
               buf + ((size_t)(miny + py) * (size_t)width + (size_t)minx) * 4u,
               (size_t)cw * 4u);
    free(buf);
    *rgba = crop;
    *x = minx;
    *w = cw;
    *h = ch;
    return 0;
}
