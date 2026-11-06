/* Reloadable AQ curve provider. Firmware decoding is shared with the offline
 * test; data access here never initializes audio or applies a hardware gain.
 * Profile selection must be supplied explicitly until native profile decoding
 * is integrated. No silent normal-curve/default fallback is invented.
 */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    oa_audio_curve *curve;
    int ready, published;
    /* ABI 3 quiesce: the decoded table is detached and freed on a worker. */
    pthread_t cleanup_thread;
    int cleanup_running;
    oa_audio_curve *cleanup_curve;
    unsigned cleanup_delay_ms;
};
static struct ctx *current;

static int curve_copy(oa_audio_curve *out)
{
    if (!current || !current->ready || !current->curve || !out) return -1;
    *out = *current->curve;
    return 0;
}
static int curve_value(unsigned volume, uint16_t *out)
{
    return current && current->ready && current->curve ?
        oa_audio_curve_value(current->curve, volume, out) : -1;
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
/* No host API calls after this returns. */
static void *cleanup_worker(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_delay_ms) {
        const struct timespec pause = {
            .tv_sec = c->cleanup_delay_ms / 1000u,
            .tv_nsec = (long)(c->cleanup_delay_ms % 1000u) * 1000000L
        };
        (void)nanosleep(&pause, NULL);
    }
    oa_audio_curve *curve = c->cleanup_curve;
    c->cleanup_curve = NULL;
    free(curve);
    return NULL;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    if (c->published || c->cleanup_running) return -EBUSY;
    c->cleanup_running = 0;
    long delay = atol(c->api->config_get(c->api->priv,
                                         "audio_curve.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    oa_audio_curve curve;
    if (read_curve(c, &curve)) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "audio_curve needs a readable 1668-byte regular AQ file and explicit integer profile");
        return -1;
    }
    if (!c->curve) {
        c->curve = malloc(sizeof *c->curve);
        if (!c->curve) return -1;
    }
    *c->curve = curve; c->ready = 1;
    int rc = c->api->publish(c->api->priv, OA_SVC_AUDIO_CURVE, 1, &iface);
    c->published = rc == 0;
    if (rc) c->ready = 0;
    return rc;
}
/* ABI 3 gate: unpublish audio.curve synchronously, mark the table unavailable,
 * detach it, then free it synchronously or on a worker after
 * audio_curve.cleanup_delay_ms. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (!c) return 0;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) { c->cleanup_running = 0; return 0; }
            if (rc != EBUSY) { c->cleanup_running = 0; return -EIO; }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }
    int published = c->published;
    c->published = 0;
    c->ready = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_AUDIO_CURVE);
    oa_audio_curve *curve = c->curve;
    c->curve = NULL;
    if (!curve) return 0;
    if (!c->cleanup_delay_ms) { free(curve); return 0; }
    c->cleanup_curve = curve;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) { c->cleanup_curve = NULL; free(curve); return -EIO; }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    int published = c->published;
    c->published = c->ready = 0;
    if (published) c->api->unpublish(c->api->priv, OA_SVC_AUDIO_CURVE);
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    free(c->cleanup_curve); c->cleanup_curve = NULL;
    free(c->curve); c->curve = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    free(c->cleanup_curve);
    free(c->curve);
    if (current == c) current = NULL;
    free(c);
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
    .suspend = m_suspend, .resume = m_resume, .quiesce = m_quiesce,
};
OA_MODULE_EXPORT(module)
