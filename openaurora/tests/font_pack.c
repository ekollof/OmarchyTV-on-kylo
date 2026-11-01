#include "openaurora/font_pack.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fwrite(data, 1, size, f) == size);
    assert(fclose(f) == 0);
}

static void write_text(const char *path, const char *text)
{
    write_file(path, text, strlen(text));
}

int main(void)
{
    char dir[] = "/tmp/oa-font-pack-XXXXXX";
    assert(mkdtemp(dir));
    char index[128], pack[128];
    snprintf(index, sizeof index, "%s/index", dir);
    snprintf(pack, sizeof pack, "%s/pack", dir);
    write_text(index, "3\n1\n2\n3\n");
    write_file(pack, "ABCDEF", 6);

    struct oa_font_pack p;
    assert(!oa_font_pack_init(&p, index, pack));
    assert(p.count == 3 && p.lengths[0] == 1 && p.offsets[1] == 1 && p.offsets[2] == 3);
    unsigned char *data = NULL;
    size_t size = 0;
    assert(!oa_font_pack_read(&p, 1, &data, &size));
    assert(size == 2 && !memcmp(data, "BC", 2));
    free(data);
    assert(oa_font_pack_read(&p, 3, &data, &size) == EINVAL);
    assert(oa_font_pack_read(&p, 0, &data, &size) == 0 && size == 1 && *data == 'A');
    free(data);
    oa_font_pack_free(&p);
    assert(p.count == 0 && !p.lengths);

    /* Total length must equal the pack size, and lengths must be positive. */
    write_text(index, "3\n1\n2\n4\n");
    assert(oa_font_pack_init(&p, index, pack) == EINVAL);
    write_text(index, "3\n1\n0\n3\n");
    assert(oa_font_pack_init(&p, index, pack) == EINVAL);
    write_text(index, "0\n");
    assert(oa_font_pack_init(&p, index, pack) == EINVAL);
    write_text(index, "3\n1\n2\n3\n");
    assert(oa_font_pack_init(&p, "/nonexistent/index", pack) != 0);
    assert(oa_font_pack_init(&p, index, "/nonexistent/pack") != 0);
    assert(!oa_font_pack_init(&p, index, pack));
    oa_font_pack_free(&p);

    unlink(index);
    unlink(pack);
    rmdir(dir);
    puts("ok font pack: index/offsets, whole-record reads, size/length/bounds rejection");
    return 0;
}
