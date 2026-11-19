#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "openaurora/source_vin.h"

void oa_source_vin_paths_init(struct oa_source_vin_paths *p)
{
    if (p)
        memset(p, 0, sizeof *p);
}

int oa_source_vin_path_set(struct oa_source_vin_paths *p, int path, int type)
{
    if (!p || type < 0)
        return -EINVAL;
    /* NdVideoSetting @0x00901c28: path > 6 (and the debug fallback) uses
     * index 0. Negative paths are not a wire case; clamp them the same way. */
    if (path < 0 || path >= (int)OA_SOURCE_VIN_PATHS)
        path = 0;
    p->type[path] = (uint32_t)type;
    return 0;
}

int oa_source_vin_path_get(const struct oa_source_vin_paths *p, int path)
{
    if (!p)
        return -EINVAL;
    if (path < 0 || path >= (int)OA_SOURCE_VIN_PATHS)
        path = 0;
    return (int)p->type[path];
}

int oa_source_vin_ready(const uint32_t words[OA_SOURCE_VIN_WORDS])
{
    if (!words) return -EINVAL;
    /* plat_vin_slot_get_status maps (w0 == 1) to its connected/ready flag.
     * The captured suspend states were w0=0 and w0=5. */
    return words[0] == 1u ? 1 : 0;
}

int oa_source_vin_poll(const struct oa_source_vin_io *io, int fd,
                       uint32_t words[OA_SOURCE_VIN_WORDS])
{
    if (!io || !io->read || fd < 0 || !words) return -EINVAL;
    uint32_t local[OA_SOURCE_VIN_WORDS];
    memset(local, 0, sizeof local);
    int rc = io->read(io->context, fd, local);
    if (rc < 0) return rc;
    memcpy(words, local, sizeof local);
    return oa_source_vin_ready(words);
}
