/* Reloadable AQ curve provider. Firmware decoding is shared with the offline
 * test; data access here never initializes audio or applies a hardware gain.
 * Profile selection must be supplied explicitly until native profile decoding
 * is integrated. No silent normal-curve/default fallback is invented.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    oa_audio_curve curve;
    int ready, published;
};
static struct ctx *current;

static int curve_copy(oa_audio_curve *out)
{
    if (!current || !current->ready || !out) return -1;
    *out = current->curve;
    return 0;
}
static int curve_value(unsigned volume, uint16_t *out)
{
    return current && current->ready ? oa_audio_curve_value(&current->curve, volume, out) : -1;
}
static const oa_audio_curve_iface iface = { curve_copy, curve_value };

static int read_curve(struct ctx *c, oa_audio_curve *curve)
{
    const char *path = c->api->config_get(c->api->priv, "audio_curve.aq", NULL);
    const char *selection = c->api->config_get(c->api->priv, "audio_curve.profile", NULL);
    if (!path || !*path || !selection || !*selection) return -1;
    char *end;
    errno = 0;
    long profile = strtol(selection, &end, 10);
    if (errno || end == selection || *end || profile < INT_MIN || profile > INT_MAX) return -1;

    /* Reject special files before reading; a FIFO in configuration must not
     * block the host dispatch loop. All reads are bounded to the fixed layout. */
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size != OA_AUDIO_AQ_SIZE) {
        (void)close(fd); return -1;
    }
    FILE *file = fdopen(fd, "rb");
    if (!file) { (void)close(fd); return -1; }
    uint8_t bytes[OA_AUDIO_AQ_SIZE + 1];
    size_t size = fread(bytes, 1, sizeof bytes, file);
    int failed = ferror(file);
    if (fclose(file)) failed = 1;
    if (failed) return -1;
    return oa_audio_curve_from_aq(curve, bytes, size, (int)profile);
}
static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api; *out = c; current = c;
    return 0;
}
static int m_start(void *v)
{
    struct ctx *c = v;
    oa_audio_curve curve;
    if (read_curve(c, &curve)) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "audio_curve needs a readable 1668-byte regular AQ file and explicit integer profile");
        return -1;
    }
    c->curve = curve; c->ready = 1;
    int rc = c->api->publish(c->api->priv, OA_SVC_AUDIO_CURVE, 1, &iface);
    c->published = rc == 0;
    if (rc) c->ready = 0;
    return rc;
}
static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_AUDIO_CURVE);
    c->published = c->ready = 0;
}
static void m_destroy(void *v)
{
    if (current == v) current = NULL;
    free(v);
}
/* Immutable data needs no device preparation; keep it available to already-held
 * consumers during paired standby. Deferred replacements read only on resume.
 */
static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }
static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "audio_curve", .version = "0.1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume,
};
OA_MODULE_EXPORT(module)
