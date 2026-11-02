#include "openaurora/binsvg.h"
#include <errno.h>
#include <string.h>

struct cursor { const uint8_t *data; size_t size, pos; };
struct parsed {
    struct oa_binsvg_element element;
    const uint8_t *fixed, *tail;
    const char *strings[9];
};
static int skip(struct cursor *c, size_t n)
{
    if (n > c->size - c->pos) return EINVAL;
    c->pos += n;
    return 0;
}
static int string(struct cursor *c, const char **out)
{
    const uint8_t *start = c->data + c->pos;
    const uint8_t *end = memchr(start, 0, c->size - c->pos);
    if (!end) return EINVAL;
    *out = (const char *)start;
    c->pos += (size_t)(end - start) + 1;
    return 0;
}
/* Layouts from BinSVGElement/SVG*::read. Animation uses variable strings;
 * SVGAnimation::seek's different fixed-tail arithmetic is deliberately unused. */
static int parse(const struct oa_binsvg *v, size_t at, struct parsed *out)
{
    if (!v || !v->data || !out || at > v->size) return EINVAL;
    if (at == v->size) return ENOENT;
    struct cursor c = {v->data,v->size,at};
    struct parsed p = {0};
    p.element.offset = at;
    unsigned tag = p.element.tag = c.data[c.pos++];
    if (tag > 11) return EINVAL;
    if (tag != 9) {
        if (string(&c,&p.element.id)) return EINVAL;
        p.fixed = c.data + c.pos;
        if (tag >= 10) {
            if (skip(&c,3)) return EINVAL;
            for (unsigned i=0;i<3;++i) if (string(&c,&p.strings[i])) return EINVAL;
            p.tail = c.data + c.pos;
            if (skip(&c,2)) return EINVAL;
            for (unsigned i=3;i<9;++i) if (string(&c,&p.strings[i])) return EINVAL;
        } else {
            static const unsigned fixed[] = {11,11,4,9,11,3,3,3,0};
            static const unsigned strings[] = {1,4,2,2,2,3,3,3,0};
            if (skip(&c,fixed[tag])) return EINVAL;
            for (unsigned i=0;i<strings[tag];++i)
                if (string(&c,&p.strings[i])) return EINVAL;
            p.tail = c.data + c.pos;
            if (tag == 1 && skip(&c,2)) return EINVAL;
        }
    }
    p.element.size = c.pos - at;
    *out = p;
    return 0;
}
int oa_binsvg_init(struct oa_binsvg *out, const void *data, size_t size)
{
    if (!out || !data || !size) return EINVAL;
    struct oa_binsvg v = {data,size};
    size_t at=0, depth=0;
    while (at < size) {
        struct parsed p;
        int rc=parse(&v,at,&p);
        if (rc) return rc;
        if (p.element.tag == 8) ++depth;
        if (p.element.tag == 9) {
            if (!depth) return EINVAL;
            --depth;
        }
        at += p.element.size;
    }
    if (depth) return EINVAL;
    *out=v;
    return 0;
}
int oa_binsvg_next(const struct oa_binsvg *v, size_t *at, struct oa_binsvg_element *out)
{
    if (!at || !out) return EINVAL;
    struct parsed p;
    int rc=parse(v,*at,&p);
    if (rc) return rc;
    *at += p.element.size;
    *out=p.element;
    return 0;
}
static int signed_word(const uint8_t *p)
{
    unsigned n = p[0] | (unsigned)p[1] << 8;
    return (int)n - (n & 0x8000 ? 65536 : 0);
}
static void rectangle(const struct parsed *p, struct oa_binsvg_rect *out)
{
    const uint8_t *d=p->fixed;
    int x=signed_word(d), y=signed_word(d+2);
    *out=(struct oa_binsvg_rect){
        .bounds={x,y,x+signed_word(d+4),y+signed_word(d+6)},
        .stroke_width=signed_word(d+8), .opacity=d[10]-(d[10]&128?256:0),
        .begin=signed_word(p->tail), .stroke=p->strings[0], .fill=p->strings[1],
        .animation=p->strings[2], .animate_transform=p->strings[3]
    };
}
int oa_binsvg_rectangle(const struct oa_binsvg *v, size_t at, struct oa_binsvg_rect *out)
{
    if (!out) return EINVAL;
    struct parsed p;
    int rc=parse(v,at,&p);
    if (rc) return rc;
    if (p.element.tag!=1) return ENOTSUP;
    rectangle(&p,out);
    return 0;
}
int oa_binsvg_find_rect(const struct oa_binsvg *v, const char *group,
                        const char *name, struct oa_binsvg_rect *out)
{
    if (!v || !v->data || !name || !*name || !out || (group && !*group)) return EINVAL;
    size_t at=0, selected_depth=0;
    while (at < v->size) {
        struct parsed p;
        int rc=parse(v,at,&p);
        if (rc) return rc;
        at += p.element.size;
        if (group) {
            if (p.element.tag == 8) {
                if (selected_depth || !strcmp(group,p.element.id)) ++selected_depth;
            } else if (p.element.tag == 9 && selected_depth) {
                if (!--selected_depth) return ENOENT;
            }
            if (!selected_depth) continue;
        }
        if (p.element.tag == 1 && !strcmp(name,p.element.id)) {
            rectangle(&p,out);
            return 0;
        }
    }
    return ENOENT;
}
