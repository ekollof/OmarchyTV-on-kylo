/* Dump named rectangles (and rectangle-capable elements) of given BinSVG
 * records from a shipped index/binary pair. Offline evidence tool; no TV,
 * no drawing. Usage: svg-rects index.txt binary.bin id... */
#include <stdio.h>
#include <stdlib.h>
#include "openaurora/svg_pack.h"
#include "openaurora/binsvg.h"

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s index binary id...\n", argv[0]); return 2; }
    struct oa_svg_pack *p = NULL;
    if (oa_svg_pack_load(&p, argv[1], argv[2]) != 0) { fprintf(stderr, "load failed\n"); return 1; }
    for (int a = 3; a < argc; a++) {
        unsigned id = (unsigned)atoi(argv[a]);
        struct oa_binsvg v;
        int rc = oa_svg_pack_record(p, id, &v);
        printf("record %u: rc=%d\n", id, rc);
        if (rc) continue;
        size_t off = 0;
        struct oa_binsvg_element e;
        while (oa_binsvg_next(&v, &off, &e) == 0) {
            printf("  tag=%u id='%s' eoff=%zu", e.tag, e.id ? e.id : "(null)", e.offset);
            struct oa_binsvg_rect r;
            if (oa_binsvg_rectangle(&v, e.offset, &r) == 0)
                printf(" rect=(%d,%d,%d,%d) stroke_w=%d opacity=%d fill='%s'",
                       r.bounds.left, r.bounds.top, r.bounds.right, r.bounds.bottom,
                       r.stroke_width, r.opacity, r.fill ? r.fill : "(null)");
            printf("\n");
        }
    }
    oa_svg_pack_free(p);
    return 0;
}
