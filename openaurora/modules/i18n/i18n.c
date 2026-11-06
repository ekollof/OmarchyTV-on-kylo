/* Reloadable vendor localization provider. Loads the langprofile Dictionary
 * and publishes keyed translations for every locale. No display/device. */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "openaurora/langprofile.h"
#include "openaurora/module.h"
#include "openaurora/services.h"

struct ctx {
    const oa_host_api *api;
    struct oa_langprofile *profile;
    char locale[8];
    int published;
    pthread_t cleanup_thread;
    int cleanup_running;
    struct oa_langprofile *cleanup_profile;
    unsigned cleanup_delay_ms;
};

static struct ctx *g_ctx;

static unsigned locale_count(void)
{
    return g_ctx && g_ctx->published
        ? oa_langprofile_locale_count(g_ctx->profile) : 0;
}

static const char *locale(unsigned index)
{
    return g_ctx && g_ctx->published
        ? oa_langprofile_locale(g_ctx->profile, index) : NULL;
}

static int find_locale(const char *code)
{
    return g_ctx && g_ctx->published
        ? oa_langprofile_locale_index(g_ctx->profile, code) : -1;
}

static const char *translate(const char *key, const char *which)
{
    if (!g_ctx || !g_ctx->published) return key ? key : "";
    return oa_langprofile_string(g_ctx->profile, key,
                                 which ? which : g_ctx->locale);
}

static const char *current_locale(void)
{
    return g_ctx && g_ctx->published ? g_ctx->locale : NULL;
}

static const oa_i18n_iface g_iface = {
    locale_count, locale, find_locale, translate, current_locale
};

static int m_init(const oa_host_api *api, void **out)
{
    if (!api || api->abi != OA_ABI_VERSION || api->size < sizeof *api) return -1;
    struct ctx *c = calloc(1, sizeof *c);
    if (!c) return -1;
    c->api = api;
    g_ctx = c;
    *out = c;
    return 0;
}

static int m_start(void *v)
{
    struct ctx *c = v;
    const char *path = c->api->config_get(c->api->priv, "i18n.profile",
        "/vendor2/mb230_langprofile.bin");
    int rc = oa_langprofile_open(&c->profile, path);
    if (rc) {
        c->api->log(c->api->priv, OA_LOG_ERROR,
                    "i18n: cannot load %s (%d)", path, rc);
        return -1;
    }
    const char *want = c->api->config_get(c->api->priv, "i18n.language", "eng");
    int index = oa_langprofile_locale_index(c->profile, want);
    if (index < 0) {
        c->api->log(c->api->priv, OA_LOG_WARN,
                    "i18n: unknown language '%s'; using eng", want);
        want = "eng";
        index = oa_langprofile_locale_index(c->profile, want);
    }
    if (index < 0) {
        oa_langprofile_close(c->profile);
        c->profile = NULL;
        return -1;
    }
    snprintf(c->locale, sizeof c->locale, "%s", want);
    long delay = atol(c->api->config_get(c->api->priv,
                                         "i18n.cleanup_delay_ms", "0"));
    c->cleanup_delay_ms = delay > 0 ? (unsigned)delay : 0;
    rc = c->api->publish(c->api->priv, OA_SVC_I18N, 1, &g_iface);
    if (rc) {
        oa_langprofile_close(c->profile);
        c->profile = NULL;
        return rc;
    }
    c->published = 1;
    c->api->log(c->api->priv, OA_LOG_INFO,
                "i18n: %u locales, language %s", locale_count(), c->locale);
    return 0;
}

/* No host API calls and no profile access after this returns. */
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
    oa_langprofile_close(c->cleanup_profile);
    c->cleanup_profile = NULL;
    return NULL;
}

/* Called before stop/unload/reload and after a failed init. A large profile
 * free is handed to a worker; the host retries until cleanup_pending clears. */
static int m_quiesce(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        for (int i = 0; i < 10; i++) {
            int rc = pthread_tryjoin_np(c->cleanup_thread, NULL);
            if (rc == 0) {
                c->cleanup_running = 0;
                return 0;
            }
            if (rc != EBUSY) {
                c->cleanup_running = 0;
                return -EIO;
            }
            const struct timespec pause = { .tv_nsec = 5000000 };
            (void)nanosleep(&pause, NULL);
        }
        return -EAGAIN;
    }
    if (c->published) {
        c->api->unpublish(c->api->priv, OA_SVC_I18N);
        c->published = 0;
    }
    if (!c->profile) return 0;
    if (!c->cleanup_delay_ms) {
        oa_langprofile_close(c->profile);
        c->profile = NULL;
        return 0;
    }
    c->cleanup_profile = c->profile;
    c->profile = NULL;
    int rc = pthread_create(&c->cleanup_thread, NULL, cleanup_worker, c);
    if (rc) {
        oa_langprofile_close(c->cleanup_profile);
        c->cleanup_profile = NULL;
        return -EIO;
    }
    c->cleanup_running = 1;
    return -EAGAIN;
}

static void m_stop(void *v)
{
    struct ctx *c = v;
    if (c->published) c->api->unpublish(c->api->priv, OA_SVC_I18N);
    c->published = 0;
    oa_langprofile_close(c->profile);
    c->profile = NULL;
}

static void m_destroy(void *v)
{
    struct ctx *c = v;
    if (c->cleanup_running) {
        (void)pthread_join(c->cleanup_thread, NULL);
        c->cleanup_running = 0;
    }
    oa_langprofile_close(c->cleanup_profile);
    oa_langprofile_close(c->profile);
    if (g_ctx == c) g_ctx = NULL;
    free(c);
}

static int m_suspend(void *v) { (void)v; return 0; }
static int m_resume(void *v) { (void)v; return 0; }

static const oa_module module = {
    .magic = OA_MODULE_MAGIC, .abi = OA_ABI_VERSION, .size = sizeof(oa_module),
    .name = "i18n", .version = "0.1",
    .init = m_init, .start = m_start, .stop = m_stop, .destroy = m_destroy,
    .suspend = m_suspend, .resume = m_resume, .quiesce = m_quiesce,
};
OA_MODULE_EXPORT(module)
