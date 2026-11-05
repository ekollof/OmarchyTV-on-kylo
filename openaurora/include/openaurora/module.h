/*
 * openaurora module ABI — C only, versioned and size-tagged in both directions.
 *
 * Rules for module authors (they are what makes unload/reload safe):
 *   1. Every callback the host makes into a module happens on the main loop thread.
 *      If you create your own thread, never call the host from it except post().
 *      Capture owner_id() on the loop thread during init() and pass it to post().
 *      stop()/destroy() MUST stop and join workers before releasing their state.
 *      The host rejects new posts once stopping begins and discards queued posts.
 *      Post userdata is borrowed: the module must retain it through dispatch or
 *      stop(), and reclaim undelivered userdata itself when stopping.
 *   2. Do not cache pointers from lookup() across a stop(); drop them with release()
 *      in stop(). The host refuses to unload a module whose services are still held,
 *      so a well-behaved module is never unloaded underneath its consumers.
 *   3. Anything you register (timer, fd watch, subscription, service) is owned by the
 *      host and is destroyed automatically when your module unloads. Do not rely on
 *      stop() having run.
 *   4. No C++ across this boundary: no std::string, no exceptions, no virtuals.
 */
#ifndef OPENAURORA_MODULE_H
#define OPENAURORA_MODULE_H

#include <stddef.h>
#include <stdint.h>

#define OA_ABI_VERSION 3u
#define OA_MODULE_MAGIC 0x4F41334Du /* "OA3M" */
#define OA_MODULE_SYMBOL "oa_module_entry"
#define OA_REQUIRES_MAX 8
#define OA_NAME_MAX 48

/* log levels */
enum { OA_LOG_ERROR = 0, OA_LOG_WARN, OA_LOG_INFO, OA_LOG_DEBUG };

/* fd watch interest / readiness */
enum { OA_WATCH_READ = 1u, OA_WATCH_WRITE = 2u, OA_WATCH_ERR = 4u };

/* module lifecycle as tracked by the host */
enum {
    OA_ST_EMPTY = 0, /* not loaded          */
    OA_ST_LOADED,    /* init() succeeded    */
    OA_ST_STARTED,   /* start() succeeded   */
    OA_ST_SUSPENDED, /* suspend() succeeded */
};

typedef struct oa_host_api oa_host_api;

struct oa_host_api {
    /* --- versioning: check size and abi before touching anything else --- */
    uint32_t size;
    uint32_t abi;
    void *priv; /* opaque host handle */

    /* --- logging; printf-style --- */
    void (*log)(void *priv, int level, const char *fmt, ...);

    /* --- services (typed interfaces by name + version) ---
     * lookup() marks the calling module as a holder, release() drops it.
     * Returns NULL if absent or if version < min_version. */
    int (*publish)(void *priv, const char *name, uint32_t version, const void *iface);
    int (*unpublish)(void *priv, const char *name);
    const void *(*lookup)(void *priv, const char *name, uint32_t min_version);
    void (*release)(void *priv, const char *name);

    /* --- events: synchronous fan-out on the loop thread --- */
    uint32_t (*subscribe)(void *priv, const char *topic, void (*cb)(const void *ev, void *ud), void *ud);
    void (*unsubscribe)(void *priv, uint32_t id);
    void (*emit)(void *priv, const char *topic, const void *ev);

    /* --- timers, milliseconds on the loop clock --- */
    uint64_t (*timer_add)(void *priv, uint32_t delay_ms, uint32_t repeat_ms, void (*cb)(void *ud), void *ud);
    void (*timer_del)(void *priv, uint64_t id);

    /* --- fd readiness, so modules never block the loop --- */
    uint32_t (*watch_add)(void *priv, int fd, unsigned events,
                          void (*cb)(void *ud, int fd, unsigned revents), void *ud);
    void (*watch_del)(void *priv, uint32_t id);

    /* --- run a callback on the loop thread (thread-safe).
     * owner must be the token captured from owner_id() during init().
     * 0 = queued, -1 = bad callback/full queue, -2 = owner inactive/unknown.
     * Posts are canceled before stop/destroy; userdata remains module-owned. --- */
    int (*post)(void *priv, uint32_t owner, void (*cb)(void *ud), void *ud);

    /* --- read-only configuration lookup --- */
    const char *(*config_get)(void *priv, const char *key, const char *def);

    /* Loop-thread only. Capture during init(), never read from a worker. */
    uint32_t (*owner_id)(void *priv);

    /* Optional loop-thread control-plane dispatch (same commands as the
     * socket/CLI). line is copied and bounded; reply is NUL-terminated. A
     * module may use this to expose gated automation; NULL on older cores. */
    int (*control)(void *priv, const char *line, char *out, size_t cap);
};

typedef struct oa_module {
    uint32_t magic;
    uint32_t abi;
    uint32_t size; /* sizeof(oa_module); loader validates before reading callbacks */

    const char *name;
    const char *version;

    /* Required services must be ready before start/resume. Providers cannot
     * normally stop while this consumer is started. These declarations and
     * lookup holds order consumer-first suspend and provider-first resume.
     * "" terminates. */
    const char *requires[OA_REQUIRES_MAX];

    /* init: connect to the host, allocate; must not touch hardware yet */
    int (*init)(const oa_host_api *api, void **ctx);
    /* start: take hardware, register services, subscribe; may fail cleanly */
    int (*start)(void *ctx);
    /* stop: release hardware, drop lookups; must be idempotent */
    void (*stop)(void *ctx);
    /* destroy: free everything init() allocated */
    void (*destroy)(void *ctx);

    /* optional: standby integration. Failure must leave state consistent for
     * retry; the host cannot roll back device changes made by a callback.
     * A replacement loaded while suspended runs
     * init/restore only; the host defers its first start until resume. Normal
     * suspended instances resume without calling start again. */
    int (*suspend)(void *ctx);
    int (*resume)(void *ctx);

    /* optional: state handoff across reload.
     * save(ctx, NULL, 0) returns the number of bytes needed; a second call fills it.
     * The second return must not exceed cap (zero means no state). Oversize or
     * allocation failure cancels reload before unloading this instance.
     * restore() runs after init() and before start() on the new instance;
     * it must not activate hardware. Nonzero (or absent with nonempty saved
     * state) rejects/destroys the replacement. No old-binary rollback is implied. */
    size_t (*save)(void *ctx, void *buf, size_t cap);
    int (*restore)(void *ctx, const void *buf, size_t len);

    /* Optional nonblocking quiescence gate, before stop/destroy and failed
     * init/start cleanup, including never-started instances. Must tolerate a
     * partial or NULL ctx and repeated calls. Zero promises that workers and
     * fallible device releases are complete; stop/destroy must then finish
     * synchronously. Nonzero retains code, ctx, posts and registrations for
     * completion/retry, even with --force. No start/resume/suspend is allowed
     * until cleanup completes. Reload may retry the gate before replacing the
     * instance, and still fails while busy. A veto may already have begun
     * quiescence: continued normal service is not promised. Loop-thread only;
     * do not block or invoke lifecycle operations recursively. */
    int (*quiesce)(void *ctx);
} oa_module;

/* The single exported symbol. Must be present or the module is rejected. */
typedef const oa_module *(*oa_module_entry_fn)(void);

/* A read-only ELF note lets --check validate ABI without loading constructors or
 * calling the entry point. It describes the compiled ABI; the runtime loader
 * still validates the actual descriptor. Keep this note when stripping modules. */
typedef struct {
    uint32_t namesz, descsz, type;
    char name[12];
    uint32_t magic, abi, size;
} oa_module_note;
_Static_assert(sizeof(oa_module_note) == 36, "module ELF note layout");
#define OA_MODULE_EXPORT(descriptor) \
    static const oa_module_note oa_abi_note \
        __attribute__((used, section(".note.openaurora"), aligned(4))) = { \
        11, 12, OA_MODULE_MAGIC, "openaurora", OA_MODULE_MAGIC, OA_ABI_VERSION, sizeof(oa_module) \
    }; \
    const oa_module *oa_module_entry(void) { return &(descriptor); }

#endif /* OPENAURORA_MODULE_H */
