/* Offline bounded reader for the target BinSVG theme pack. Format recovered
 * from BinSVGElement/SVG* read/seek methods in Aurora, not from byte scanning.
 * Prints animation descriptors with their containing record hash and offset.
 * Firmware is local-only; this tool never accesses a TV or renders anything. */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cursor { const unsigned char *data; size_t size, pos; };
static unsigned nodes, animations;
static int rectangles;
static int skip(struct cursor *c, size_t n)
{
    if (n > c->size - c->pos) return -1;
    c->pos += n;
    return 0;
}
static const char *string(struct cursor *c)
{
    const unsigned char *p = c->data + c->pos;
    const unsigned char *end = memchr(p, 0, c->size - c->pos);
    if (!end) return NULL;
    c->pos += (size_t)(end - p) + 1;
    return (const char *)p;
}
static int word(struct cursor *c, unsigned *out)
{
    if (c->size - c->pos < 2) return -1;
    *out = c->data[c->pos] | (unsigned)c->data[c->pos + 1] << 8;
    c->pos += 2;
    return 0;
}
static void field(const char *s)
{
    putchar('\t');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 32 || *p == 127 || *p == '\\') printf("\\x%02x", *p);
        else putchar(*p);
    }
}
static int element(struct cursor *c, uint32_t hash, size_t base)
{
    size_t start = c->pos;
    unsigned tag = c->data[c->pos++];
    nodes++;
    if (tag == 9) return 0; /* SVGGroupEnd: one byte, no id */
    if (tag > 11) return -1;
    const char *id = string(c);
    if (!id) return -1;
    if (tag == 10 || tag == 11) {
        if (c->pos == c->size) return -1;
        unsigned subtype = c->data[c->pos++], offset, begin;
        if (word(c, &offset) != 0) return -1;
        const char *link = string(c), *attribute = string(c), *transform = string(c);
        if (!link || !attribute || !transform || word(c, &begin) != 0) return -1;
        const char *values[6]; /* duration, fill, from, to, calcMode, keySplines */
        for (unsigned i = 0; i < 6; i++) if (!(values[i] = string(c))) return -1;
        if (!rectangles) {
        printf("0x%08" PRIx32 "\t%zu\t%u", hash, base + start, tag);
        field(id); printf("\t%u\t%d", subtype, (int)offset - (offset & 0x8000 ? 65536 : 0));
        field(link); field(attribute); field(transform);
        printf("\t%d", (int)begin - (begin & 0x8000 ? 65536 : 0));
        for (unsigned i = 0; i < 6; i++) field(values[i]);
        putchar('\n');
        }
        animations++;
        return 0;
    }
    unsigned fixed, strings, tail = 0;
    switch (tag) {
    case 0: fixed = 11; strings = 1; break; /* line */
    case 1: fixed = 11; strings = 4; tail = 2; break; /* rectangle */
    case 2: fixed = 4; strings = 2; break; /* image */
    case 3: fixed = 9; strings = 2; break; /* circle */
    case 4: fixed = 11; strings = 2; break; /* ellipse */
    case 5: case 6: case 7: fixed = 3; strings = 3; break; /* poly/path */
    case 8: fixed = 0; strings = 0; break; /* group start */
    default: return -1;
    }
    size_t data = c->pos;
    const char *values[4];
    if (skip(c, fixed) != 0) return -1;
    for (unsigned i = 0; i < strings; i++) if (!(values[i] = string(c))) return -1;
    if (skip(c, tail)) return -1;
    if (rectangles && tag == 1) {
        printf("0x%08" PRIx32 "\t%zu", hash, base + start); field(id);
        for (unsigned i = 0; i < 5; i++) {
            unsigned v = c->data[data+i*2] | (unsigned)c->data[data+i*2+1] << 8;
            printf("\t%d", (int)v - (v & 0x8000 ? 65536 : 0));
        }
        unsigned opacity = c->data[data+10];
        printf("\t%d", (int)opacity - (opacity & 0x80 ? 256 : 0));
        for (unsigned i = 0; i < 4; i++) field(values[i]);
        putchar('\n');
    }
    return 0;
}
int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[3], "--rectangles")) rectangles = 1;
    else if (argc != 3) {
        fprintf(stderr, "usage: %s telefunken18_svg.txt telefunken18_svg.bin [--rectangles]\n", argv[0]);
        return 2;
    }
    FILE *index = fopen(argv[1], "r"), *pack = fopen(argv[2], "rb");
    unsigned char *data = NULL;
    int result = 1;
    size_t record = 0, offset = 0;
    if (!index || !pack || fseek(pack, 0, SEEK_END) != 0) goto done;
    long end = ftell(pack);
    if (end < 0 || end > 64 * 1024 * 1024 || fseek(pack, 0, SEEK_SET) != 0) goto done;
    size_t total = (size_t)end;
    data = malloc(total ? total : 1);
    if (!data || fread(data, 1, total, pack) != total) goto done;
    char line[256], extra;
    size_t count;
    if (!fgets(line, sizeof line, index) || sscanf(line, "%zu %c", &count, &extra) != 1 || count > 100000)
        goto done;
    if (rectangles) puts("record_hash\tpack_offset\tid\tx\ty\tw\th\tword4\topacity_raw_signed\tstring0\tstring1\tstring2\tstring3");
    else puts("record_hash\tpack_offset\ttag\tid\tsubtype\toffset\txlink\tattribute\ttransform\tbegin\tduration\tfill\tfrom\tto\tcalc_mode\tkey_splines");
    for (; record < count; record++) {
        uint32_t hash;
        size_t length;
        if (!fgets(line, sizeof line, index) ||
            sscanf(line, "%" SCNx32 " %zu %c", &hash, &length, &extra) != 2 ||
            length > total - offset || !length) goto done;
        struct cursor c = { data + offset, length, 0 };
        while (c.pos < c.size) {
            size_t before = c.pos;
            if (element(&c, hash, offset) != 0) {
                fprintf(stderr, "invalid element at pack offset %zu (tag %u)\n",
                         offset + before, c.data[before]);
                goto done;
            }
        }
        offset += length;
    }
    if (offset != total || fgets(line, sizeof line, index)) goto done;
    if (ferror(index) || ferror(pack) || ferror(stdout)) goto done;
    fprintf(stderr, "%zu records, %u elements, %u animations, %zu bytes consumed\n",
             count, nodes, animations, offset);
    result = 0;
done:
    if (result) fprintf(stderr, "pack parse failed near record %zu, record offset %zu\n", record, offset);
    if (index) fclose(index);
    if (pack) fclose(pack);
    free(data);
    return result;
}
