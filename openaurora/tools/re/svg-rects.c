/* Dump named rectangles (and rectangle-capable elements) of given BinSVG
 * records from a shipped index/binary pair. Offline evidence tool; no TV,
 * no drawing. Usage: svg-rects index.txt binary.bin id...
 *
 * Arguments are parsed strictly: a non-numeric, negative, empty or
 * out-of-range id is rejected rather than silently becoming record 0. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "openaurora/svg_pack.h"
#include "openaurora/binsvg.h"

static int parse_id(const char *s, unsigned *out)
{
    if (!s || *s < '0' || *s > '9')
        return -1;
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(s, &end, 10);
    if (errno || !end || *end != '\0' || v > 0xfffffffful)
        return -1;
    *out = (unsigned)v;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s index binary id...\n", argv[0]); return 2; }
    struct oa_svg_pack *p = NULL;
    if (oa_svg_pack_load(&p, argv[1], argv[2]) != 0) { fprintf(stderr, "load failed\n"); return 1; }
    int status = 0;
    for (int a = 3; a < argc; a++) {
        unsigned id = 0;
        if (parse_id(argv[a], &id) != 0) {
            fprintf(stderr, "invalid record id '%s'\n", argv[a]);
            status = 2;
            continue;
        }
        struct oa_binsvg v;
        int rc = oa_svg_pack_record(p, id, &v);
        printf("record %u: rc=%d\n", id, rc);
        if (rc) { status = 1; continue; }
        size_t off = 0;
        struct oa_binsvg_element e;
        for (;;) {
            rc = oa_binsvg_next(&v, &off, &e);
            if (rc == ENOENT)
                break;               /* clean end of record */
            if (rc != 0) {
                fprintf(stderr, "record %u: element iteration failed (%d)\n", id, rc);
                status = 1;
                break;
            }
            printf("  tag=%u id='%s' eoff=%zu", e.tag, e.id ? e.id : "(null)", e.offset);
            struct oa_binsvg_rect r;
            if (oa_binsvg_rectangle(&v, e.offset, &r) == 0)
                printf(" rect=(%d,%d,%d,%d) stroke_w=%d opacity=%d stroke='%s' fill='%s'",
                       r.bounds.left, r.bounds.top, r.bounds.right, r.bounds.bottom,
                       r.stroke_width, r.opacity, r.stroke ? r.stroke : "(null)",
                       r.fill ? r.fill : "(null)");
            printf("\n");
        }
    }
    oa_svg_pack_free(p);
    return status;
}
