/* Pure model of SVGListBoxBase::getRowPosition. No UI or SVG I/O. */
#include "openaurora/row_position.h"

#include <errno.h>
#include <stddef.h>

int oa_row_position(const oa_row_position_io *io,
                    const oa_row_position_fields *f, int index, int out[4])
{
    int rect[4] = { 0, 0, 0, 0 }, p1[2], p2[2];
    if (!io || !io->query || !f || !out || index < 0)
        return -EINVAL;
    if (io->query(io->ud, rect) != 0)
        return -EIO;

    if (!f->e8) {
        /* Branch at 0x010bbb5e: x is +0x204, step comes from +0x1f4.
         * 010bbb8e..010bbb98 computes
         * (rect.x1 - rect.x0) - f204 - f208 + p1.x; p1.x is f204, so the
         * horizontal term reduces to (rect.x1 - rect.x0) - f208. Do not add
         * the row step here: that was a model defect caught 2026-11-17. */
        int step = f->f1f4 * index + f->f1fc;
        p1[0] = f->f204;
        p1[1] = step;
        p2[0] = (rect[2] - rect[0]) - f->f208;
        p2[1] = step + f->f1f8;
    } else {
        int step;
        if (!f->e9)
            step = f->f20c * index + f->f204;
        else
            step = rect[2] - f->f204 - (f->f20c * index + f->f20c);
        p1[0] = step;
        p1[1] = f->f1fc;
        p2[0] = step + f->f210;
        /* 010bbb1a..010bbb1c: (y1-y0) - f1fc - f200 + f1fc; the f1fc terms
         * cancel and only the -f200 inset remains. */
        p2[1] = (rect[3] - rect[1]) - f->f200;
    }

    /* EASIRect(EASIPoint p1, EASIPoint p2). */
    out[0] = p1[0];
    out[1] = p1[1];
    out[2] = p2[0];
    out[3] = p2[1];
    return 0;
}
