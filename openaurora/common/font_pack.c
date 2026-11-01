#include "openaurora/font_pack.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PACK_MAX_RECORDS 256u
#define PACK_MAX_RECORD (64u << 20)

void oa_font_pack_free(struct oa_font_pack *pack)
{
    if (!pack) return;
    free(pack->lengths);
    free(pack->offsets);
    free(pack->pack_path);
    memset(pack, 0, sizeof *pack);
}

int oa_font_pack_init(struct oa_font_pack *pack, const char *index_path,
                      const char *pack_path)
{
    if (!pack || !index_path || !pack_path) return EINVAL;
    memset(pack, 0, sizeof *pack);
    FILE *index = fopen(index_path, "r");
    if (!index) return errno ? errno : EIO;
    unsigned count;
    if (fscanf(index, "%u", &count) != 1 || count == 0 || count > PACK_MAX_RECORDS) {
        fclose(index);
        return EINVAL;
    }
    unsigned *lengths = calloc(count, sizeof *lengths);
    unsigned *offsets = calloc(count, sizeof *offsets);
    if (!lengths || !offsets) {
        free(lengths);
        free(offsets);
        fclose(index);
        return ENOMEM;
    }
    unsigned long long total = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned length;
        if (fscanf(index, "%u", &length) != 1 || length == 0 || length > PACK_MAX_RECORD) {
            free(lengths);
            free(offsets);
            fclose(index);
            return EINVAL;
        }
        if (total > 0xffffffffull - length) {
            free(lengths);
            free(offsets);
            fclose(index);
            return EOVERFLOW;
        }
        offsets[i] = (unsigned)total;
        lengths[i] = length;
        total += length;
    }
    fclose(index);
    if (total > (unsigned long long)LONG_MAX) {
        free(lengths);
        free(offsets);
        return EOVERFLOW;
    }
    FILE *file = fopen(pack_path, "rb");
    if (!file) {
        int rc = errno ? errno : EIO;
        free(lengths);
        free(offsets);
        return rc;
    }
    if (fseek(file, 0, SEEK_END) != 0 || ftell(file) < 0) {
        fclose(file);
        free(lengths);
        free(offsets);
        return EIO;
    }
    long size = ftell(file);
    fclose(file);
    if (size < 0 || (unsigned long long)size != total) {
        free(lengths);
        free(offsets);
        return EINVAL;
    }
    char *path = strdup(pack_path);
    if (!path) {
        free(lengths);
        free(offsets);
        return ENOMEM;
    }
    pack->count = count;
    pack->lengths = lengths;
    pack->offsets = offsets;
    pack->pack_path = path;
    return 0;
}

int oa_font_pack_read(const struct oa_font_pack *pack, unsigned id,
                      unsigned char **data, size_t *size)
{
    if (!pack || !data || !size || !pack->lengths || !pack->offsets ||
        !pack->pack_path || id >= pack->count) return EINVAL;
#if LONG_MAX < UINT_MAX
    if (pack->offsets[id] > LONG_MAX) return EOVERFLOW;
#endif
    *data = NULL;
    *size = 0;
    unsigned char *buf = malloc(pack->lengths[id]);
    if (!buf) return ENOMEM;
    FILE *file = fopen(pack->pack_path, "rb");
    if (!file) {
        int rc = errno ? errno : EIO;
        free(buf);
        return rc;
    }
    if (fseek(file, (long)pack->offsets[id], SEEK_SET) != 0 ||
        fread(buf, 1, pack->lengths[id], file) != pack->lengths[id]) {
        fclose(file);
        free(buf);
        return EIO;
    }
    if (fclose(file) != 0) {
        free(buf);
        return EIO;
    }
    *data = buf;
    *size = pack->lengths[id];
    return 0;
}
