/* image_decode — decode vendor-downloaded PNGs into straight RGBA.
 *
 * Firmware has hal::PngImageDecoderImpl (0xb2f6a0..) and a shared
 * /vendor/lib/libs/libpng16.so.16 (1.6.28). The downloaded launcher assets are
 * extensionless PNG/JPEG files in /buffer/downloaded_images; this module covers
 * the PNG subset through libpng's simplified file API. No hardware is touched,
 * no scaling or caching is done, and the service may be stopped/reloaded.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

/* png_image layout from libpng 1.6's simplified API. The opaque pointer is
 * followed by seven uint32 fields and a 64-byte message. */
typedef struct {
    void *opaque;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint32_t flags;
    uint32_t colormap_entries;
    uint32_t warning_or_error;
    char message[64];
} png_image_v1;
_Static_assert(sizeof(png_image_v1) == (sizeof(void *) == 8 ? 104u : 96u),
    "libpng simplified png_image layout changed");

#define PNG_IMAGE_VERSION_V1 1u
#define PNG_FORMAT_RGBA_V1 0x03u
#define PNG_IMAGE_MAX_DIM 8192u

typedef int (*png_begin_file_fn)(png_image_v1 *, const char *);
typedef int (*png_finish_read_fn)(png_image_v1 *, const void *, void *, int32_t,
    void *);
typedef void (*png_image_free_fn)(png_image_v1 *);

struct ctx {
    const oa_host_api *api;
    void *lib;
    void *zlib;
    png_begin_file_fn begin_file;
    png_finish_read_fn finish_read;
    png_image_free_fn image_free;
    int published;
};
static struct ctx *g_ctx;

static int load_symbol(void *lib, const char *name, void *out, size_t out_size)
{
    void *sym = dlsym(lib, name);
    if (!sym || out_size != sizeof sym) return -1;
    memcpy(out, &sym, out_size);
    return 0;
}

static void release_pixels(void *rgba) { free(rgba); }

static int decode_png(const char *path, int *w, int *h, unsigned char **rgba)
{
    struct ctx *c = g_ctx;
    png_image_v1 image;
    unsigned char *pixels;
    size_t len;
    int32_t stride;
    if (!c || !c->begin_file || !c->finish_read || !c->image_free ||
        !path || !path[0] || !w || !h || !rgba)
        return -1;
    *w = 0;
    *h = 0;
    *rgba = NULL;
    memset(&image, 0, sizeof image);
    image.version = PNG_IMAGE_VERSION_V1;
    if (!c->begin_file(&image, path)) {
        c->image_free(&image);
        return -1;
    }
    if (image.width == 0 || image.height == 0 ||
        image.width > PNG_IMAGE_MAX_DIM || image.height > PNG_IMAGE_MAX_DIM ||
        image.width > SIZE_MAX / 4u / image.height) {
        c->image_free(&image);
        return -1;
    }
    len = (size_t)image.width * (size_t)image.height * 4u;
    stride = (int32_t)(image.width * 4u);
    pixels = malloc(len);
    if (!pixels) {
        c->image_free(&image);
        return -1;
    }
    image.format = PNG_FORMAT_RGBA_V1;
    if (!c->finish_read(&image, NULL, pixels, stride, NULL)) {
        free(pixels);
        c->image_free(&image);
        return -1;
    }
    c->image_free(&image);
    *w = (int)image.width;
    *h = (int)image.height;
    *rgba = pixels;
    return 0;
}

static const oa_image_decode_iface g_iface = { decode_png, release_pixels };

static void close_libs(struct ctx *c)
{
    if (c->lib) dlclose(c->lib);
    if (c->zlib) dlclose(c->zlib);
    c->lib = NULL;
    c->zlib = NULL;
    c->begin_file = NULL;
    c->finish_read = NULL;
    c->image_free = NULL;
}

/* The TV's loader does not expose /vendor/lib/libs by default. Try the normal
 * name first (host and any configured loader path), then preload the vendor
 * zlib soname before opening the absolute vendor libpng path. */
static void *open_libpng(const char *name, void **zlib_out)
{
    void *lib = dlopen(name, RTLD_NOW | RTLD_LOCAL);
    if (lib) return lib;
    void *zlib = dlopen("/vendor/lib/libs/libz.so", RTLD_NOW | RTLD_GLOBAL);
    lib = dlopen("/vendor/lib/libs/libpng16.so.16", RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        if (zlib) dlclose(zlib);
        return NULL;
    }
    *zlib_out = zlib;
    return lib;
}

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api)
        return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    *out = c;
    g_ctx = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    const char *name = c->api->config_get(c->api->priv, "image.lib",
        "libpng16.so.16");
    if (!name || !name[0]) return -1;
    c->lib = open_libpng(name, &c->zlib);
    if (!c->lib) {
        const char *why = dlerror();
        c->api->log(c->api->priv, OA_LOG_ERROR, "image_decode: %s",
            why ? why : "libpng load failed");
        return -1;
    }
    if (load_symbol(c->lib, "png_image_begin_read_from_file",
            &c->begin_file, sizeof c->begin_file) ||
        load_symbol(c->lib, "png_image_finish_read",
            &c->finish_read, sizeof c->finish_read) ||
        load_symbol(c->lib, "png_image_free",
            &c->image_free, sizeof c->image_free)) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
            "image_decode: libpng simplified API missing");
        close_libs(c);
        return -1;
    }
    if (c->api->publish(c->api->priv, OA_SVC_IMAGE_DECODE, 1, &g_iface)) {
        close_libs(c);
        return -1;
    }
    c->published = 1;
    return 0;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_IMAGE_DECODE);
    c->published = 0;
    close_libs(c);
}

static void m_destroy(void *v)
{
    if (g_ctx == v) g_ctx = NULL;
    free(v);
}

static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "image_decode", .version = "1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
};
OA_MODULE_EXPORT(module)
