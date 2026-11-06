#include "openaurora/picture_config.h"

#include <errno.h>
#include <string.h>

static int source_ok(int source)
{
    return source >= 0 && source < OA_PICTURE_SOURCES;
}

static int word_get(const unsigned char *blob, int offset, int *out)
{
    if (!blob || !out || offset < 0 ||
        (size_t)offset + sizeof *out > OA_PICTURE_BYTES)
        return EINVAL;
    memcpy(out, blob + offset, sizeof *out);
    return 0;
}

static int word_set(unsigned char *blob, int offset, int value)
{
    if (!blob || offset < 0 ||
        (size_t)offset + sizeof value > OA_PICTURE_BYTES)
        return EINVAL;
    memcpy(blob + offset, &value, sizeof value);
    return 0;
}

static int mode_offset(int source)
{
    return source * OA_PICTURE_STRIDE + OA_PICTURE_OFF_MODE;
}

static int zoom_offset(int source, int full_mode, int zoom, int setting)
{
    int full = full_mode != 0;
    if (setting && !full && zoom == OA_PICTURE_ZOOM_FULL) full = 1;
    return source * OA_PICTURE_STRIDE +
           (full ? OA_PICTURE_OFF_ZOOM_FULL : OA_PICTURE_OFF_ZOOM_NORMAL);
}

static int energy_offset(int source, int mode, int device3d)
{
    if (!source_ok(source) || mode < 0 || mode > 4 ||
        device3d < 0 || device3d > 1)
        return -1;
    return source * OA_PICTURE_STRIDE + OA_PICTURE_OFF_ENERGY +
           mode * OA_PICTURE_ENERGY_MODE_STRIDE +
           device3d * OA_PICTURE_ENERGY_3D_STRIDE;
}

int oa_picture_get_mode(const unsigned char *blob, int source, int *out)
{
    if (!source_ok(source)) return EINVAL;
    return word_get(blob, mode_offset(source), out);
}

int oa_picture_set_mode(unsigned char *blob, int source, int mode)
{
    if (!source_ok(source) || mode < 0 || mode > 4) return EINVAL;
    return word_set(blob, mode_offset(source), mode);
}

int oa_picture_get_zoom(const unsigned char *blob, int source, int full_mode,
                        int *out)
{
    if (!source_ok(source)) return EINVAL;
    return word_get(blob, zoom_offset(source, full_mode, 0, 0), out);
}

int oa_picture_set_zoom(unsigned char *blob, int source, int full_mode,
                        int zoom)
{
    if (!source_ok(source) || zoom < 0 || zoom > 0x7f) return EINVAL;
    return word_set(blob, zoom_offset(source, full_mode, zoom, 1), zoom);
}

int oa_picture_get_energy(const unsigned char *blob, int source, int mode,
                          int device3d, int *out)
{
    int off = energy_offset(source, mode, device3d);
    if (off < 0) return EINVAL;
    return word_get(blob, off, out);
}

int oa_picture_set_energy(unsigned char *blob, int source, int mode,
                          int device3d, int value)
{
    int off = energy_offset(source, mode, device3d);
    if (off < 0 || value < 0 || value > 0xff) return EINVAL;
    return word_set(blob, off, value);
}

int oa_picture_get_config(const unsigned char *blob, int source,
                          const unsigned char **out)
{
    if (!blob || !out || !source_ok(source)) return EINVAL;
    *out = blob + source * OA_PICTURE_STRIDE;
    return 0;
}

int oa_picture_set_config(unsigned char *blob, int source,
                          const unsigned char *config)
{
    if (!blob || !config || !source_ok(source)) return EINVAL;
    memcpy(blob + source * OA_PICTURE_STRIDE, config, OA_PICTURE_STRIDE);
    return 0;
}
