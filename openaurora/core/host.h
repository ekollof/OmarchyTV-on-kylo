#ifndef OPENAURORA_HOST_H
#define OPENAURORA_HOST_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#include "openaurora/module.h"
#include "openaurora/services.h"

#define MAX_MODULES 32
#define MAX_TIMERS 256
#define MAX_WATCHES 64
#define MAX_SUBS 256
#define MAX_SVCS 48
#define MAX_HOLDS 128
#define MAX_CLIENTS 8
#define MAX_POSTS 64
#define MAX_STATE (64 * 1024)
#define TOPIC_MAX 48
#define MAX_PHASE_DEPS (MAX_SVCS + OA_REQUIRES_MAX)

typedef struct {
    uint64_t id;
    uint64_t due;
    uint32_t repeat_ms;
    void (*cb)(void *ud);
    void *ud;
    uint32_t owner;
    int active;
} oa_timer;

typedef struct {
    uint32_t id;
    int fd;
    unsigned events;
    void (*cb)(void *ud, int fd, unsigned revents);
    void *ud;
    uint32_t owner;
    int active;
} oa_watch;

typedef struct {
    uint32_t id;
    char topic[TOPIC_MAX];
    void (*cb)(const void *ev, void *ud);
    void *ud;
    uint32_t owner;
    int active;
} oa_sub;

typedef struct {
    char name[OA_NAME_MAX];
    uint32_t version;
    const void *iface;
    uint32_t owner;
    int used;
} oa_svc;

/* a module holding a pointer to another module's service */
typedef struct {
    char name[OA_NAME_MAX];
    uint32_t holder;
    int used;
} oa_hold;

typedef struct {
    char service[OA_NAME_MAX];
    char provider[OA_NAME_MAX]; /* fallback while suspended provider is unpublished */
} oa_phase_dep;

typedef struct {
    char name[OA_NAME_MAX];
    char path[256];
    void *dl;
    const oa_module *desc;
    void *ctx;
    int state;
    int start_on_resume; /* reloaded while suspended: init/restore only until wake */
    int cleanup_pending; /* quiescence veto: only stop/unload may retry */
    int init_failed; /* retained failed init must never be started */
    int rejected; /* retained failed replacement must never be started */
    int start_attempted; /* stop is required even after a partial start */
    uint32_t id;
    unsigned ndeps;
    oa_phase_dep deps[MAX_PHASE_DEPS];
    unsigned char *blob; /* state carried across a reload */
    size_t blob_len;
    char err[160];
} oa_mod;

typedef struct {
    void (*cb)(void *);
    void *ud;
    uint32_t owner;
} oa_post;

typedef struct {
    uint32_t id;
    uint32_t owner;
    uint64_t due;
    oa_sched_step step;
    void *ud;
    int active;
} oa_sched_task;

typedef struct {
    oa_host_api api;
    oa_sleep_iface sleep_iface;
    oa_sched_iface sched_iface;
    oa_sched_task tasks[OA_SCHED_SLOTS];
    int sched_depth;

    oa_mod mods[MAX_MODULES];
    oa_timer timers[MAX_TIMERS];
    oa_watch watches[MAX_WATCHES];
    oa_sub subs[MAX_SUBS];
    oa_svc svcs[MAX_SVCS];
    oa_hold holds[MAX_HOLDS];

    uint32_t next_id;
    uint32_t cur_module; /* module currently executing, 0 = host */
    int log_level;
    int running;
    int sleep_cycle; /* pins lifecycle/DSOs through prepare, entry and restore */
    int shutting_down; /* stop/unload only; load/start/reload/suspend/resume refuse */
    int reload_requested; /* clean exit for an oa-supervise relaunch */
    volatile sig_atomic_t stop_requested;

    char heartbeat_path[256];
    uint64_t heartbeat_due;

    int selfpipe[2];
    pthread_mutex_t post_mu;
    oa_post posts[MAX_POSTS];
    /* Worker-visible state protected by post_mu; workers never read mods[]. */
    uint32_t post_owners[MAX_MODULES];
    int nposts;

    char modules_dir[256];
    char config_path[256];
    /* Immutable initialization snapshot, owned by this host. First duplicate
     * wins as before; module reload does not reread the configuration file.
     * Keep this above the shipped profile count (49 at 2026-11-09); overflow
     * now logs a warning instead of silently dropping later keys. */
    struct { char k[64], v[512]; } config[128];
    int config_count;
    /* Host-owned, transient skin policy; never written back to config. */
    char skin_selected[OA_NAME_MAX], skin_requested[OA_NAME_MAX], skin_module[OA_NAME_MAX];
    char skin_pending[OA_NAME_MAX];
    uint64_t skin_timer, skin_deadline;
    uint32_t skin_owner;
    int skin_phase, skin_error, skin_fallback;
} oa_host;

uint64_t oa_now_ms(void);
int oa_skin_request(oa_host *h, const char *name);
int oa_skin_bootstrap(oa_host *h);
void oa_skin_status(oa_host *h, char *out, size_t cap);
/* Run a skin request queued from inside a module callback. Loop thread only,
 * after callbacks/timers have unwound. */
void oa_skin_pump(oa_host *h);

/* host.c */
int oa_host_init(oa_host *h, const char *modules_dir, const char *config);
int oa_runtime_check(const char *modules_dir);
/* Runs the loop, then stops and unloads every module. A quiescence veto keeps
 * the loop alive until the completion post, timer or watch has been dispatched
 * and a later stop/unload succeeds. Returns -1 if a module remains loaded.
 * Does not dlclose an instance whose gate is still busy. */
int oa_host_ready(oa_host *h);
int oa_host_run(oa_host *h);
void oa_host_quit(oa_host *h);
void oa_host_signal_stop(oa_host *h); /* async-signal-safe, after init */
void oa_host_cleanup_owned(oa_host *h, uint32_t module_id);
int oa_host_enable_posts(oa_host *h, uint32_t module_id);
void oa_host_disable_posts(oa_host *h, uint32_t module_id);
void oa_host_dispatch_posts(oa_host *h);
/* One slice of each due task, at most eight tasks. Nested calls do nothing.
 * Standby (sleep_cycle) does not slice. */
void oa_host_sched_pump(oa_host *h);
int oa_host_service_held_by_others(oa_host *h, const char *name, uint32_t owner);
int oa_host_service_exists(oa_host *h, const char *name);
oa_mod *oa_host_find_module(oa_host *h, const char *name);
oa_mod *oa_host_find_module_by_id(oa_host *h, uint32_t id);

/* module.c */
oa_mod *oa_module_load(oa_host *h, const char *path_or_name);
int oa_module_start(oa_host *h, oa_mod *m);
int oa_module_stop(oa_host *h, oa_mod *m);
int oa_module_unload(oa_host *h, const char *name, int force);
int oa_module_reload(oa_host *h, const char *name, int force);
int oa_module_suspend_all(oa_host *h);
int oa_module_resume_all(oa_host *h);
/* Internal host coordinator; backend runs only after all active modules quiesce.
 * Restores only this cycle's transitions, including after prepare/entry failure.
 * Does not itself write power state or claim hardware readiness. */
int oa_module_sleep_cycle(oa_host *h, uint32_t entry_owner,
                          int (*enter)(void *), void *ud, oa_sleep_result *result);
int oa_module_standby_cycle(oa_host *h, uint32_t entry_owner,
                            int (*enter)(void *), void *ud, oa_sleep_result *result);
const char *oa_state_name(int st);

/* control.c */
int oa_control_start(oa_host *h, const char *sock_path);
void oa_control_stop(oa_host *h);
/* Additive module batches: one load/start per call, run by the host loop.
 * pending() is true while awake work remains, so poll() must not block. */
void oa_control_pump(oa_host *h);
int oa_control_pending(oa_host *h);
/* returns 0 ok, -1 error, 1 = quit requested */
int oa_control_command(oa_host *h, char *line, char *out, size_t outlen);

#endif /* OPENAURORA_HOST_H */
