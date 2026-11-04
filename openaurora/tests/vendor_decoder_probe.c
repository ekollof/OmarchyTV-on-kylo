/* Offline vendor-decoder probe.
 *
 * This is an ARM LD_PRELOAD constructor for aurora.elf under qemu. It stops
 * before the vendor main(), constructs the shipped decoder class with the
 * vendor MemoryBufferSource/MutableImage helpers, decodes one file and writes
 * a raw result. No TV, HAL device or vendor UI is touched.
 *
 *   OA_VENDOR_FORMAT=png|jpeg OA_VENDOR_INPUT=FILE OA_VENDOR_OUTPUT=FILE
 *
 * Result: magic, rc, width, height, rowlen, input_size, then rowlen*height
 * bytes. PNG/JPEG vendor output is BGRA or palette indices; the host test
 * normalizes it.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC 0x56444543u /* "VDEC" */

typedef void (*ctor_fn)(void *);
typedef void (*mbs_ctor)(void *, const void *, unsigned);
typedef int (*decode_fn)(void *, void *, void *, const void *, void *, void *,
    void *);
typedef void *(*getpix_fn)(void *, int, int);

static void *need(const char *name)
{
    void *p = dlsym(RTLD_DEFAULT, name);
    if (!p) {
        dprintf(2, "vendor-decoder-probe: missing %s\n", name);
        _exit(3);
    }
    return p;
}

static unsigned char *read_file(const char *path, size_t *len)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) _exit(4);
    struct stat st;
    if (fstat(fd, &st) || st.st_size <= 0) _exit(5);
    unsigned char *buf = malloc((size_t)st.st_size);
    if (!buf) _exit(6);
    size_t at = 0;
    while (at < (size_t)st.st_size) {
        ssize_t n = read(fd, buf + at, (size_t)st.st_size - at);
        if (n <= 0) _exit(7);
        at += (size_t)n;
    }
    close(fd);
    *len = (size_t)st.st_size;
    return buf;
}

static void fn_void1(void *sym, void *a)
{
    void (*fn)(void *);
    memcpy(&fn, &sym, sizeof fn);
    fn(a);
}
static void fn_mbs(void *sym, void *a, const void *b, unsigned c)
{
    void (*fn)(void *, const void *, unsigned);
    memcpy(&fn, &sym, sizeof fn);
    fn(a, b, c);
}
static int fn_decode(void *sym, void *a, void *b, void *c, void *d, void *e,
    void *f, void *g)
{
    int (*fn)(void *, void *, void *, void *, void *, void *, void *);
    memcpy(&fn, &sym, sizeof fn);
    return fn(a, b, c, d, e, f, g);
}
static int fn_int1(void *sym, void *a)
{
    int (*fn)(void *);
    memcpy(&fn, &sym, sizeof fn);
    return fn(a);
}
static void fn_colour(void *sym, void *a, int model)
{
    void (*fn)(void *, int);
    memcpy(&fn, &sym, sizeof fn);
    fn(a, model);
}
static void fn_props(void *sym, void *a, int w, int h, int xr, int yr,
    void *model, int transparent)
{
    void (*fn)(void *, int, int, int, int, void *, int);
    memcpy(&fn, &sym, sizeof fn);
    fn(a, w, h, xr, yr, model, transparent);
}
static void *fn_getpix(void *sym, void *a, int x, int y)
{
    void *(*fn)(void *, int, int);
    memcpy(&fn, &sym, sizeof fn);
    return fn(a, x, y);
}

__attribute__((constructor)) static void probe(void)
{
    const char *format = getenv("OA_VENDOR_FORMAT");
    const char *input = getenv("OA_VENDOR_INPUT");
    const char *output = getenv("OA_VENDOR_OUTPUT");
    if (!format || !input || !output) _exit(2);
    int jpeg = !strcmp(format, "jpeg");
    size_t input_len = 0;
    unsigned char *data = read_file(input, &input_len);

    const char *ctor = jpeg ? "_ZN3hal16JpegImageDecoderC1Ev"
                            : "_ZN3hal19PngImageDecoderImplC1Ev";
    const char *init = jpeg ? "_ZN3hal16JpegImageDecoder10initializeEv"
                            : "_ZN3hal19PngImageDecoderImpl10initializeEv";
    const char *decode = jpeg
        ? "_ZN3hal16JpegImageDecoder11decodeImageERNS_9AnySourceERNS_15AnyMutableImageEPKNS_15ImagePropertiesEPFvPS3_PvS9_ES9_S9_"
        : "_ZN3hal19PngImageDecoderImpl11decodeImageERNS_9AnySourceERNS_15AnyMutableImageEPKNS_15ImagePropertiesEPFvPS3_PvS9_ES9_S9_";

    void *source = calloc(1, 0x20);
    void *image = calloc(1, 0x100);
    void *decoder = calloc(1, 0x200);
    void *colour = calloc(1, 8);
    void *properties = calloc(1, 0x80);
    if (!source || !image || !decoder || !colour || !properties) _exit(8);
    const char *model_text = getenv("OA_VENDOR_MODEL");
    int model = model_text && *model_text ? atoi(model_text) : 3;
    fn_colour(need("_ZN4lynx11ColourModelC1ENS0_5ModelE"), colour, model);
    fn_props(need("_ZN3hal15ImagePropertiesC1EiiiiN4lynx11ColourModelEb"),
        properties, 0, 0, 0, 0, colour, 0);
    fn_mbs(need("_ZN3hal18MemoryBufferSourceC1EPKvj"), source, data,
        (unsigned)input_len);
    fn_void1(need("_ZN3hal12MutableImageC1Ev"), image);
    fn_void1(need(ctor), decoder);
    fn_void1(need(init), decoder);
    int rc = fn_decode(need(decode), decoder, source, image, properties,
        NULL, NULL, NULL);

    void **vtable = *(void ***)image;
    int width = fn_int1(vtable[0x08 / 4], image);
    int height = fn_int1(vtable[0x0c / 4], image);
    int rowlen = fn_int1(vtable[0x10 / 4], image);
    unsigned char *pixels = width > 0 && height > 0 && rowlen > 0
        ? fn_getpix(vtable[0x58 / 4], image, 0, 0) : NULL;

    int fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) _exit(9);
    uint32_t header[6] = { MAGIC, (uint32_t)rc, (uint32_t)width,
        (uint32_t)height, (uint32_t)rowlen, (uint32_t)input_len };
    if (write(fd, header, sizeof header) != (ssize_t)sizeof header) _exit(10);
    if (pixels && rowlen > 0 && height > 0 &&
        write(fd, pixels, (size_t)rowlen * (size_t)height) !=
        (ssize_t)((size_t)rowlen * (size_t)height))
        _exit(11);
    close(fd);
    _exit(0);
}
