/* image_decode module: real vendor-downloaded PNGs (RGB, RGBA and colormap)
 * must decode to the same straight-RGBA bytes as libpng/ImageMagick, and the
 * service must survive stop/start. Offline only; no TV or display access. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "host.h"
#include "openaurora/services.h"

struct expected {
    const char *name;
    int w, h;
    uint64_t fnv;
};
static const struct expected EXPECTED[] = {
    { "launcher_app_thumb336", 400, 225, 0x0627f025fe6f6509ULL },
    { "launcher_app_thumb354", 400, 225, 0x232173008794f7b4ULL },
    { "launcher_app_thumb355", 400, 225, 0x51285c632edf638dULL },
    { "launcher_app_thumb356", 400, 225, 0xfd66750dc6cf36e4ULL },
    { "launcher_app_thumb368", 400, 225, 0x96168ee6b7ce7111ULL },
    { "launcher_app_thumb385", 400, 225, 0x361f6ed1116a8c59ULL },
    { "launcher_app_thumb476", 400, 225, 0x6533d1a94012211fULL },
    { "launcher_app_thumb495", 400, 225, 0x8bc67b81c16e809fULL },
    { "launcher_app_thumb61",  400, 225, 0x12815d59be310b91ULL },
};

static uint64_t fnv1a(const unsigned char *data, size_t len)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    struct stat st;
    if (stat(argv[2], &st) || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "skipping: %s unavailable\n", argv[2]);
        return 77;
    }
    oa_host host;
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = -1;
    char out[256];
    assert(!command(&host, "load image_decode", out, sizeof out));
    assert(!command(&host, "start image_decode", out, sizeof out));
    const oa_image_decode_iface *dec =
        host.api.lookup(&host, OA_SVC_IMAGE_DECODE, 1);
    assert(dec && dec->decode_png && dec->release);

    char path[512];
    for (size_t i = 0; i < sizeof EXPECTED / sizeof EXPECTED[0]; i++) {
        int w = -1, h = -1;
        unsigned char *rgba = (unsigned char *)1;
        snprintf(path, sizeof path, "%s/%s", argv[2], EXPECTED[i].name);
        assert(!dec->decode_png(path, &w, &h, &rgba));
        assert(w == EXPECTED[i].w && h == EXPECTED[i].h && rgba);
        size_t len = (size_t)w * (size_t)h * 4u;
        assert(fnv1a(rgba, len) == EXPECTED[i].fnv);
        if (!strcmp(EXPECTED[i].name, "launcher_app_thumb336")) {
            /* RGB source: libpng expands to opaque RGBA. */
            assert(rgba[0] == 26 && rgba[1] == 152 && rgba[2] == 255 && rgba[3] == 255);
        }
        dec->release(rgba);
    }

    int w = -1, h = -1;
    unsigned char *rgba = (unsigned char *)1;
    assert(dec->decode_png("/nonexistent/oa-image.png", &w, &h, &rgba) < 0);
    assert(w == 0 && h == 0 && !rgba);
    snprintf(path, sizeof path, "%s/image_decode.c", argv[2]);
    assert(dec->decode_png(path, &w, &h, &rgba) < 0);
    assert(w == 0 && h == 0 && !rgba);
    assert(dec->decode_png("", &w, &h, &rgba) < 0);
    assert(dec->decode_png(NULL, &w, &h, &rgba) < 0);
    host.api.release(&host, OA_SVC_IMAGE_DECODE);

    assert(!command(&host, "stop image_decode", out, sizeof out));
    assert(!host.api.lookup(&host, OA_SVC_IMAGE_DECODE, 1));
    assert(!command(&host, "start image_decode", out, sizeof out));
    dec = host.api.lookup(&host, OA_SVC_IMAGE_DECODE, 1);
    assert(dec);
    snprintf(path, sizeof path, "%s/launcher_app_thumb61", argv[2]);
    assert(!dec->decode_png(path, &w, &h, &rgba));
    assert(w == 400 && h == 225 && rgba);
    dec->release(rgba);
    host.api.release(&host, OA_SVC_IMAGE_DECODE);
    puts("  ok   image decode: nine downloaded PNGs match libpng RGBA, errors reject and service reloads");
    return 0;
}
