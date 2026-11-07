/*
 * control.c — the control plane: a unix socket plus stdin.
 *
 * This is what makes the modular design usable: modules can be loaded, started,
 * stopped, unloaded and reloaded on a running TV without touching the screen
 * logic, and anything that goes wrong is one command away from being replaced.
 */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "openaurora/audio_speaker.h"
#include "openaurora/channels.h"
#include "openaurora/events.h"
#include "openaurora/menu_model.h"
#include "openaurora/module.h"
#include "openaurora/ui_engine.h"
#include "openaurora/ui_omarchy.h"
#include "host.h"

/* Full module/service tables fit without truncating normal diagnostics. */
#define CONTROL_REPLY_MAX 16384
#define CONTROL_QUEUE_MAX (2 * CONTROL_REPLY_MAX)

struct input {
    size_t len;
    int discard;
    char buf[512];
};

struct client {
    int fd;
    uint32_t watch;
    unsigned events;
    int eof;
    struct input input;
    size_t sent, pending;
    char output[CONTROL_QUEUE_MAX];
};

/* Additive non-blocking module batches: one lifecycle operation per loop turn,
 * so a stack of modules does not monopolise the control loop. Each individual
 * init/start/stop/unload/reload still runs to completion on the loop thread —
 * the batch only bounds the burst, it cannot make one slow module call
 * asynchronous. A failed entry is logged and the next one still runs. */
#define BATCH_MAX 64
#define BATCH_NAME 128
enum { BATCH_LOAD, BATCH_START, BATCH_STOP, BATCH_UNLOAD, BATCH_RELOAD };
static struct {
    char name[BATCH_NAME];
    int op;
} g_batch[BATCH_MAX];
static unsigned g_batch_head, g_batch_count;

static int batch_enqueue(const char *name, int op)
{
    if (!name || !*name || strlen(name) >= BATCH_NAME)
        return 0;
    if (g_batch_count >= BATCH_MAX)
        return 0;
    unsigned tail = (g_batch_head + g_batch_count) % BATCH_MAX;
    memcpy(g_batch[tail].name, name, strlen(name) + 1);
    g_batch[tail].op = op;
    g_batch_count++;
    return 1;
}

/* One module operation per call. The loop runs this before polling and stops
 * waiting in poll() while work remains, so entries advance one per turn. */
void oa_control_pump(oa_host *h)
{
    if (!h || h->sleep_cycle || h->shutting_down || g_batch_count == 0)
        return;
    char name[BATCH_NAME];
    int op = g_batch[g_batch_head].op;
    memcpy(name, g_batch[g_batch_head].name, sizeof name);
    g_batch_head = (g_batch_head + 1) % BATCH_MAX;
    g_batch_count--;

    oa_mod *m = oa_host_find_module(h, name);
    switch (op) {
    case BATCH_LOAD:
        if (!m && !oa_module_load(h, name))
            h->api.log(h, OA_LOG_ERROR, "batch: load %s failed", name);
        break;
    case BATCH_START:
        if (!m)
            m = oa_module_load(h, name);
        if (!m || oa_module_start(h, m) != 0)
            h->api.log(h, OA_LOG_ERROR, "batch: start %s failed", name);
        break;
    case BATCH_STOP:
        if (!m || oa_module_stop(h, m) != 0)
            h->api.log(h, OA_LOG_ERROR, "batch: stop %s failed", name);
        break;
    case BATCH_UNLOAD:
        if (oa_module_unload(h, name, 0) != 0)
            h->api.log(h, OA_LOG_ERROR, "batch: unload %s failed", name);
        break;
    case BATCH_RELOAD:
        if (oa_module_reload(h, name, 0) != 0)
            h->api.log(h, OA_LOG_ERROR, "batch: reload %s failed", name);
        break;
    }
}

int oa_control_pending(oa_host *h)
{
    if (!h || h->sleep_cycle || h->shutting_down)
        return 0;
    return g_batch_count != 0;
}

static struct {
    int listen_fd;
    uint32_t listen_watch;
    uint32_t stdin_watch;
    oa_host *owner;
    struct input stdin_input;
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    dev_t dev;
    ino_t ino;
    struct client clients[MAX_CLIENTS];
} g_ctl = { .listen_fd = -1 };

/* ------------------------------------------------------------------ output */

static void out(oa_host *h, int fd, const char *fmt, ...)
{
    (void)h;
    char line[CONTROL_REPLY_MAX];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if ((size_t)n > sizeof line - 1)
        n = sizeof line - 1;
    ssize_t r = write(fd, line, (size_t)n);
    (void)r;
}

/* ---------------------------------------------------------------- commands */

struct reply {
    char *buf;
    size_t cap;
    size_t used;
};

static int decimal(const char *text, uint64_t *out)
{
    if (!text || !*text) return -1;
    for (const char *p = text; *p; p++) if (*p < '0' || *p > '9') return -1;
    char *end;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || *end || value > UINT64_MAX) return -1;
    *out = (uint64_t)value;
    return 0;
}
static int signed_decimal64(const char *text, int64_t *out)
{
    if (!text || !*text) return -1;
    const char *p = text;
    if (*p == '-') ++p;
    if (!*p) return -1;
    for (; *p; ++p) if (*p < '0' || *p > '9') return -1;
    char *end;
    errno = 0;
    long long value = strtoll(text, &end, 10);
    if (errno || *end) return -1;
    *out = (int64_t)value;
    return 0;
}
static int signed_decimal(const char *text,int32_t *out)
{
    if(!text || !*text)return -1;
    const char *p=text;if(*p=='-')++p;
    if(!*p)return -1;
    for(;*p;++p)if(*p<'0' || *p>'9')return -1;
    char *end;errno=0;long long value=strtoll(text,&end,10);
    if(errno || *end || value<INT32_MIN || value>INT32_MAX)return -1;
    *out=(int32_t)value;return 0;
}

static int hex_nibble(char c)
{
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    if(c>='A' && c<='F')return c-'A'+10;
    return -1;
}

static int hex_decode(const char *text,uint8_t *out,size_t cap,size_t *len)
{
    if(!text || !out || !len)return -1;
    size_t n=strlen(text);
    if(!n || (n&1u) || n/2u>cap)return -1;
    for(size_t i=0;i<n;i+=2){
        int hi=hex_nibble(text[i]),lo=hex_nibble(text[i+1]);
        if(hi<0 || lo<0)return -1;
        out[i/2]=(uint8_t)((hi<<4)|lo);
    }
    *len=n/2u;
    return 0;
}

/* snprintf returns the length it wanted, which must never become a cursor or
 * be subtracted from capacity after truncation. Keep only bytes actually stored. */
static void append(struct reply *r, const char *fmt, ...)
{
    if (r->used >= r->cap || r->cap - r->used <= 1)
        return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(r->buf + r->used, r->cap - r->used, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    size_t avail = r->cap - r->used - 1;
    r->used += (size_t)n < avail ? (size_t)n : avail;
}

static void cmd_list(oa_host *h, char *outbuf, size_t outlen)
{
    struct reply r = { outbuf, outlen, 0 };
    int count = 0;
    for (int i = 0; i < MAX_MODULES; i++) {
        oa_mod *m = &h->mods[i];
        if (m->state == OA_ST_EMPTY)
            continue;
        append(&r, "%-14s %-9s id=%-3u %s\n", m->name,
               oa_state_name(m->state), m->id, m->path);
        count++;
    }
    if (!count) append(&r, "(no modules loaded)\n");
}

static void cmd_status(oa_host *h, const char *name, char *outbuf, size_t outlen)
{
    struct reply reply = { outbuf, outlen, 0 };
    oa_mod *m = oa_host_find_module(h, name);
    if (!m) {
        append(&reply, "no such module: %s\n", name);
        return;
    }
    append(&reply, "name    %s\nstate   %s\nversion %s\npath    %s\nrequires",
           m->name, oa_state_name(m->state), m->desc->version ? m->desc->version : "?", m->path);
    for (int i = 0; i < OA_REQUIRES_MAX; i++) {
        const char *r = m->desc->requires[i];
        if (!r || !*r)
            break;
        append(&reply, " %s", r);
    }
    append(&reply, "\ncleanup_pending %d\nactivation_rejected %d\nservices",
           m->cleanup_pending, m->init_failed || m->rejected);
    for (int i = 0; i < MAX_SVCS; i++)
        if (h->svcs[i].used && h->svcs[i].owner == m->id)
            append(&reply, " %s:v%u", h->svcs[i].name, h->svcs[i].version);
    append(&reply, "\n");
}

static uint64_t fnv1a64(const unsigned char *data, size_t len)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

/*
 * returns 0 ok, -1 error, 1 = quit
 */
int oa_control_command(oa_host *h, char *line, char *outbuf, size_t outlen)
{
    if (outlen) outbuf[0] = 0;
    char *save = NULL;
    char *cmd = strtok_r(line, " \t\r\n", &save);
    if (!cmd || !*cmd)
        return 0;
    char *a1 = strtok_r(NULL, " \t\r\n", &save);
    char *a2 = strtok_r(NULL, " \t\r\n", &save);
    char *a3 = strtok_r(NULL, " \t\r\n", &save);
    char *a4 = strtok_r(NULL, " \t\r\n", &save);
    char *a5 = strtok_r(NULL, " \t\r\n", &save);
    char *a6 = strtok_r(NULL, " \t\r\n", &save);
    char *a7 = strtok_r(NULL, " \t\r\n", &save);
    int force = a2 && (!strcmp(a2, "--force") || !strcmp(a1 ? a1 : "", "--force"));
    if (a1 && !strcmp(a1, "--force")) {
        a1 = a2;
        force = 1;
    }

    if (!strcmp(cmd, "help")) {
        snprintf(outbuf, outlen,
                 "commands:\n"
                 "  list                     module table\n"
                 "  status <name>            details\n"
                 "  load <name|path>         dlopen + init\n"
                 "  load-batch <names...>    queue loads, one per loop turn\n"
                 "  start-batch <names...>   queue load+start, one per loop turn\n"
                 "  stop-batch <names...>    queue stops, one per turn (consumers first)\n"
                 "  unload-batch <names...>  queue unloads, one per turn (consumers first)\n"
                 "  reload-batch <names...>  queue hot reloads, one per turn (consumers first)\n"
                 "  start <name>             start (after deps)\n"
                 "  stop <name>              stop\n"
                 "  unload <name> [--force]  stop + destroy + dlclose\n"
                 "  reload <name> [--force]  hot reload with state handoff\n");
        if (outlen) {
            size_t used = strlen(outbuf);
            if (used < outlen)
                snprintf(outbuf + used, outlen - used,
                 "  suspend | resume         standby hooks for all modules\n"
                 "  standby                  coordinated power service request\n"
                 "  skin [name]              query/select ui_<name>, with vendor fallback\n"
                 "  nav-state                query retained settings route state\n"
                 "  nav-operation <id>       queue a launcher operation ID\n"
                 "  nav-result <signed-id>   queue a completed modal result\n"
                 "  nav-take                 consume one queued navigation request\n"
                 "  nav-set-tab <signed-id>  set retained tab (no enqueue/draw)\n"
                 "  nav-set-item <signed-id> set retained item (no enqueue/draw)\n"
                 "  nav-set-submenu <0-255>  set retained submenu byte only\n"
                 "  list-state               query owned SVG-list selection state\n"
                 "  list-key <code>          feed a full-width list key code\n"
                 "  key <code>               feed an IR key frame (RC/NEC code)\n"
                 "  hotel-state             persisted hotel tab inputs\n"
                 "  amp-profile             read immutable amplifier profile inputs\n"
                 "  amp-state               query audio.amp init lifecycle\n"
                 "  amp-init                submit one amplifier Init\n"
                 "  amp-mute <0|1>          set the vendor amplifier mute level\n"
                 "  amp-balance <0-100>     set AD82587 regs 4/5 balance\n"
                 "  audio-curve <volume>    read published AQ curve value\n"
                 "  audio-curve-dump        dump 101 published AQ curve values\n"
                 "  audio-speaker <level> <balance>  speaker clamp/HAL balance map\n"
                 "  launcher-items <parent|all> [deleted] [outofsync]\n"
                 "                           settingsDB LauncherItem rows\n"
                 "  tuner-state             scan/lock/signal state\n"
                 "  tuner-scan <net>        start fixture scan 1-5\n"
                 "  tuner-cancel            cancel a running scan\n"
                 "  tuner-select <handle>   select a completed service handle\n"
                 "  epg-query <svc> <from> <to>  bounded EPG events\n"
                 "  epg-ingest <hex>        feed one complete EIT section\n"
                 "  epg-clear               drop cached EPG events\n"
                 "  epg-count               cached EPG event count\n"
                 "  epg-nownext <svc> <now_ms>  current/next EPG event\n"
                 "  channel-ingest <hex>    feed ACTIVECHANNELLIST XML (runtime order)\n"
                 "  channel-clear           drop the runtime active list\n"
                 "  channel-state           runtime/stored list source and counts\n"
                 "  channel-list            runtime active-list rows\n"
                 "  pvr-list                PVR reservations\n"
                 "  pvr-add <svc> <from> <to> [repeat start_pad end_pad]\n"
                 "                           add reservation\n"
                 "  pvr-cancel <id>         cancel scheduled reservation\n"
                 "  pvr-record <id>         start scheduled recording\n"
                 "  pvr-stop <id>           complete active recording\n"
                 "  service-list [stats|name|channel] [i]  inventory rows\n"
                 "  image-decode <path>      decode PNG/JPEG through image.decode\n"
                 "  ui-state                query ui.portal screen\n"
                 "  theme-state             query ui.theme pack count\n"
                 "  profile-state           query ui.profile facts\n"
                 "  panel-state             query panel requested percent\n"
                 "  anim-state              query ui.anim enabled/fault\n"
                 "  clock-state             query ui.clock current ms\n"
                 "  ticker-state            query ticker saved tick count\n"
                 "  console-state           ui_console saved selection\n"
                 "  omarchy-state           ui_omarchy saved skin state\n"
                 "  emit-key <id>           emit a host KEY press\n"
                 "  menu-state <dialog>     query ui.menu dialogs/branches\n"
                 "  ui-action <action>      apply a JSON menu action (open:/setting:/set:)\n"
                 "  picture-state <source>  read picture mode/zoom/energy 0-19\n"
                 "  ui-icon <name>          load/report a ui.engine asset\n"
                 "  plat-state              query plat ready/stage errors\n"
                 "  display-info            query display dimensions/bpp/frames\n"
                 "  source-select <type>    select a VIN source type 0-25\n"
                 "  source-state            query current source type/availability\n"
                 "  audio-state             query audio enabled/volume\n"
                 "  audio-volume <0-100>    set diagnostic audio volume\n"
                 "  cec-state               query cec owner/audio policy\n"
                 "  cec-open                acquire the raw CEC device\n"
                 "  cec-close               release the raw CEC device\n"
                 "  cec-send <hex>          submit one CEC frame (TV initiator)\n"
                 "  mixer-query <0-5>        submit read-only gain query\n"
                 "  mixer-result <ticket>    collect query result\n"
                 "  mixer-cancel <ticket>    discard/drain query\n"
                 "  loglevel <0-3>\n"
                 "  quit\n");
        }
        return 0;
    }
    if (!strcmp(cmd, "skin")) {
        if (a2 || force) { snprintf(outbuf, outlen, "failed errno %d\n", EINVAL); return -1; }
        if (!a1) { oa_skin_status(h, outbuf, outlen); return 0; }
        if (oa_skin_request(h, a1)) {
            /* A skin calling api->control("skin ...") runs with cur_module set;
             * queue it until the callback and timers have unwound. */
            if (errno == EBUSY && h->cur_module && !h->skin_timer &&
                !h->skin_pending[0]) {
                snprintf(h->skin_pending, sizeof h->skin_pending, "%s", a1);
                snprintf(outbuf, outlen, "pending %s\n", a1);
                return 0;
            }
            snprintf(outbuf, outlen, "failed errno %d\n", errno);
            return -1;
        }
        snprintf(outbuf, outlen, "pending %s\n", a1);
        return 0;
    }
    if (!strcmp(cmd, "console-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        oa_mod *m = oa_host_find_module(h, "ui_console");
        if (!m || !m->ctx || !m->desc->save) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        unsigned char sel = 0;
        uint32_t prev = h->cur_module;
        h->cur_module = m->id;
        size_t n = m->desc->save(m->ctx, &sel, sizeof sel);
        h->cur_module = prev;
        if (n != sizeof sel) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen, "console selection %u\n", (unsigned)sel);
        return 0;
    }
    if (!strcmp(cmd, "omarchy-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        oa_mod *m = oa_host_find_module(h, "ui_omarchy");
        if (!m || !m->ctx || !m->desc->save) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        oa_omarchy_state st;
        memset(&st, 0, sizeof st);
        uint32_t prev = h->cur_module;
        h->cur_module = m->id;
        size_t n = m->desc->save(m->ctx, &st, sizeof st);
        h->cur_module = prev;
        if (n != sizeof st || st.magic != OA_OMARCHY_STATE_MAGIC ||
            st.version != OA_OMARCHY_STATE_VERSION) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen,
                 "omarchy colors %d bg %d %d %d %d accent %d %d %d "
                 "layout %d %d %d %d %d screen %d %d %d\n",
                 (int)st.have_colors, (int)st.bg[0], (int)st.bg[1],
                 (int)st.bg[2], (int)st.bg[3], (int)st.accent[0],
                 (int)st.accent[1], (int)st.accent[2], (int)st.have_layout,
                 (int)st.layout[0], (int)st.layout[1], (int)st.layout[2],
                 (int)st.layout[3], (int)st.have_screen, (int)st.screen[0],
                 (int)st.screen[1]);
        return 0;
    }
    if (!strcmp(cmd, "emit-key")) {
        uint64_t code = 0;
        if (force || !a1 || a2 || decimal(a1, &code) || code > 255) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        oa_key_event ev = {0};
        ev.id = (uint8_t)code;
        ev.kind = OA_KEY_PRESS;
        h->api.emit(h, OA_TOPIC_KEY, &ev);
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if (!strcmp(cmd, "ticker-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_clock_iface *clock = h->api.lookup(h, OA_SVC_CLOCK, 1);
        if (!clock) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        h->api.release(h, OA_SVC_CLOCK);
        oa_mod *m = oa_host_find_module(h, "ticker");
        if (!m || !m->ctx || !m->desc->save) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        uint64_t ticks = 0;
        uint32_t prev = h->cur_module;
        h->cur_module = m->id;
        size_t n = m->desc->save(m->ctx, &ticks, sizeof ticks);
        h->cur_module = prev;
        if (n != sizeof ticks) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen, "ticker ticks %" PRIu64 "\n", ticks);
        return 0;
    }
    if (!strcmp(cmd, "clock-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_ui_clock_iface *clock =
            h->api.lookup(h, OA_SVC_UI_CLOCK, 1);
        if (!clock) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        uint32_t now = clock->now_ms();
        h->api.release(h, OA_SVC_UI_CLOCK);
        snprintf(outbuf, outlen, "clock now %u\n", (unsigned)now);
        return 0;
    }
    if (!strcmp(cmd, "anim-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_ui_anim_iface *a =
            h->api.lookup(h, OA_SVC_UI_ANIM, 3);
        if (!a) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int on = a->enabled();
        int fault = a->fault();
        h->api.release(h, OA_SVC_UI_ANIM);
        snprintf(outbuf, outlen, "anim enabled %d fault %d\n", on, fault);
        return 0;
    }
    if (!strcmp(cmd, "panel-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_panel_iface *p = h->api.lookup(h, OA_SVC_PANEL, 1);
        if (!p) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int percent = p->get_backlight_percent();
        h->api.release(h, OA_SVC_PANEL);
        snprintf(outbuf, outlen, "panel percent %d\n", percent);
        return 0;
    }
    if (!strcmp(cmd, "profile-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_ui_profile_iface *p =
            h->api.lookup(h, OA_SVC_UI_PROFILE, 1);
        if (!p) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        struct oa_tab_profile profile;
        int error = p->copy(&profile);
        h->api.release(h, OA_SVC_UI_PROFILE);
        if (error) {
            snprintf(outbuf, outlen, "failed errno %d\n", error);
            return -1;
        }
        snprintf(outbuf, outlen,
                 "profile connectivity %u enabled %u hotel %u enabled %u present %u/%u\n",
                 profile.connectivity_raw, profile.connectivity_enabled,
                 profile.hotel_raw, profile.hotel_enabled,
                 profile.connectivity_present, profile.hotel_present);
        return 0;
    }
    if (!strcmp(cmd, "theme-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_ui_theme_iface *t = h->api.lookup(h, OA_SVC_UI_THEME, 1);
        if (!t) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        unsigned count = t->count();
        h->api.release(h, OA_SVC_UI_THEME);
        snprintf(outbuf, outlen, "theme count %u\n", count);
        return 0;
    }
    if (!strcmp(cmd, "ui-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_portal_iface *p =
            h->api.lookup(h, OA_SVC_PORTAL, 1);
        if (!p) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int screen = p->screen();
        h->api.release(h, OA_SVC_PORTAL);
        snprintf(outbuf, outlen, "portal screen %d\n", screen);
        return 0;
    }
    if (!strcmp(cmd, "menu-state")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_ui_menu_iface *m = h->api.lookup(h, OA_SVC_UI_MENU, 1);
        if (!m) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        const struct oa_menu_doc *doc = m->document();
        const struct oa_menu_dialog *d = doc ? m->dialog(a1) : NULL;
        unsigned dialogs = doc ? doc->dialog_count : 0;
        unsigned branches = d ? d->branch_count : 0;
        h->api.release(h, OA_SVC_UI_MENU);
        if (!d) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENOENT);
            return -1;
        }
        snprintf(outbuf, outlen, "menu %s dialogs %u branches %u\n",
                 a1, dialogs, branches);
        return 0;
    }
    if (!strcmp(cmd, "ui-action")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_portal_iface *p =
            h->api.lookup(h, OA_SVC_PORTAL, 1);
        if (!p) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int rc = p->menu_action(a1);
        h->api.release(h, OA_SVC_PORTAL);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", rc);
            return -1;
        }
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if (!strcmp(cmd, "picture-state")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        char *end = NULL;
        long source = strtol(a1, &end, 0);
        if (*a1 == '\0' || !end || *end != '\0' ||
            source < 0 || source > 19) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_picture_settings_iface *p =
            h->api.lookup(h, OA_SVC_PICTURE_SETTINGS, 1);
        if (!p) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int mode = -1, zoom = -1, energy = -1;
        int rc = p->get_mode((int)source, &mode);
        if (!rc) rc = p->get_zoom((int)source, &zoom);
        if (!rc) rc = p->get_energy((int)source, mode < 0 ? 0 : mode, 0,
                                    &energy);
        h->api.release(h, OA_SVC_PICTURE_SETTINGS);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", rc);
            return -1;
        }
        snprintf(outbuf, outlen,
                 "picture source %ld mode %d zoom %d energy %d\n",
                 source, mode, zoom, energy);
        return 0;
    }
    if (!strcmp(cmd, "ui-icon")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_ui_engine_iface *e =
            h->api.lookup(h, OA_SVC_UI_ENGINE, 2);
        if (!e) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        const oa_ui_icon *ic = e->icon(a1);
        h->api.release(h, OA_SVC_UI_ENGINE);
        if (!ic) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENOENT);
            return -1;
        }
        snprintf(outbuf, outlen, "icon %s %dx%d\n", ic->name, ic->w, ic->h);
        return 0;
    }
    if (!strcmp(cmd, "source-select")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        char *end = NULL;
        long type = strtol(a1, &end, 0);
        if (*a1 == '\0' || !end || *end != '\0' || type < 0 || type > 25) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_source_iface *src = h->api.lookup(h, OA_SVC_SOURCE, 1);
        if (!src) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int rc = src->select_type((int)type);
        h->api.release(h, OA_SVC_SOURCE);
        if (rc == -2) {
            snprintf(outbuf, outlen, "failed disabled\n");
            return -1;
        }
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if (!strcmp(cmd, "source-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_source_iface *src = h->api.lookup(h, OA_SVC_SOURCE, 1);
        if (!src) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int type = src->current_type();
        int available = src->is_available();
        h->api.release(h, OA_SVC_SOURCE);
        snprintf(outbuf, outlen, "source type %d available %d\n", type,
                 available);
        return 0;
    }
    if (!strcmp(cmd, "audio-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_audio_iface *a = h->api.lookup(h, OA_SVC_AUDIO, 1);
        if (!a) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int enabled = a->is_available ? a->is_available() : 0;
        int volume = a->get_volume ? a->get_volume() : -1;
        h->api.release(h, OA_SVC_AUDIO);
        snprintf(outbuf, outlen, "audio enabled %d volume %d\n",
                 enabled, volume);
        return 0;
    }
    if (!strcmp(cmd, "audio-volume")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        char *end = NULL;
        long value = strtol(a1, &end, 0);
        if (*a1 == '\0' || !end || *end != '\0' || value < 0 || value > 100) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_audio_iface *a = h->api.lookup(h, OA_SVC_AUDIO, 1);
        if (!a) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int rc = a->set_volume((int)value);
        h->api.release(h, OA_SVC_AUDIO);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if (!strcmp(cmd, "plat-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_plat_iface *p = h->api.lookup(h, OA_SVC_PLAT, 1);
        if (!p) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int ready = p->is_ready ? p->is_ready() : -1;
        int errors = p->stage_errors ? p->stage_errors() : -1;
        h->api.release(h, OA_SVC_PLAT);
        snprintf(outbuf, outlen, "plat ready %d errors 0x%x\n", ready, errors);
        return 0;
    }
    if (!strcmp(cmd, "display-info")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_display_iface *d = h->api.lookup(h, OA_SVC_DISPLAY, 2);
        if (!d) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int w = 0, hgt = 0, bpp = 0, frames = 0;
        int rc = d->info(&w, &hgt, &bpp, &frames);
        h->api.release(h, OA_SVC_DISPLAY);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen, "display %dx%d %dbpp %d frames\n",
                 w, hgt, bpp, frames);
        return 0;
    }
    if (!strcmp(cmd, "image-decode")) {
        if (force || a2 || !a1) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_image_decode_iface *dec =
            h->api.lookup(h, OA_SVC_IMAGE_DECODE, 2);
        if (!dec) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int w = 0, hgt = 0, rc;
        unsigned char *rgba = NULL;
        rc = dec->decode_png(a1, &w, &hgt, &rgba);
        if (rc && dec->decode_jpeg) {
            int png_rc = rc;
            int jpeg_rc = dec->decode_jpeg(a1, &w, &hgt, &rgba);
            /* A non-image file should report EINVAL, not the JPEG-only
             * "unsupported" status from the second attempt. */
            rc = jpeg_rc ? (png_rc == -EINVAL ? -EINVAL : jpeg_rc) : 0;
        }
        /* Match the service's documented bounds and the ui_engine consumer.
         * A replacement provider's dimensions must not overflow the ARM32
         * byte count or make the control-plane hash walk unbounded memory. */
        if (!rc && (w > 8192 || hgt > 8192 ||
            (w > 0 && hgt > 0 &&
             (uint64_t)(unsigned)w * (unsigned)hgt * 4u > 64u * 1024u * 1024u)))
            rc = -EOVERFLOW;
        if (rc || w <= 0 || hgt <= 0 || !rgba) {
            if (rgba) dec->release(rgba);
            h->api.release(h, OA_SVC_IMAGE_DECODE);
            snprintf(outbuf, outlen, "failed errno %d\n", rc ? -rc : EINVAL);
            return -1;
        }
        uint64_t hash = fnv1a64(rgba, (size_t)w * (size_t)hgt * 4u);
        dec->release(rgba);
        h->api.release(h, OA_SVC_IMAGE_DECODE);
        snprintf(outbuf, outlen, "image %dx%d fnv %016" PRIx64 "\n",
                 w, hgt, hash);
        return 0;
    }
    if(!strcmp(cmd,"tuner-state")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_tuner_iface *tuner=h->api.lookup(h,OA_SVC_TUNER,1);
        if(!tuner){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        struct oa_tuner_status st;
        int rc=tuner->status(&st);
        h->api.release(h,OA_SVC_TUNER);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,
            "network %u state %u progress %u lock %u services %u selected %u "
            "error %d strength %u quality %u ber %.2f snr %.2f unc %u\n",
            st.network,st.scan_state,st.progress,st.locked,st.services_found,
            st.selected_handle,st.error,st.signal_strength,st.signal_quality,
            (double)st.ber,(double)st.snr,st.unc);
        return 0;
    }
    if(!strcmp(cmd,"tuner-scan")) {
        if(force || !a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t network=0;
        if(decimal(a1,&network) || network<1 || network>5){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_tuner_iface *tuner=h->api.lookup(h,OA_SVC_TUNER,1);
        if(!tuner){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=tuner->scan_start((uint8_t)network,1);
        h->api.release(h,OA_SVC_TUNER);
        snprintf(outbuf,outlen,rc?"failed errno %d\n":"ok\n",rc?-rc:0);
        return rc? -1:0;
    }
    if(!strcmp(cmd,"tuner-cancel")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_tuner_iface *tuner=h->api.lookup(h,OA_SVC_TUNER,1);
        if(!tuner){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=tuner->scan_cancel();
        h->api.release(h,OA_SVC_TUNER);
        snprintf(outbuf,outlen,rc?"failed errno %d\n":"ok\n",rc?-rc:0);
        return rc? -1:0;
    }
    if(!strcmp(cmd,"tuner-select")) {
        if(force || !a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t handle=0;
        if(decimal(a1,&handle) || handle>UINT32_MAX){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_tuner_iface *tuner=h->api.lookup(h,OA_SVC_TUNER,1);
        if(!tuner){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=tuner->select((uint32_t)handle);
        h->api.release(h,OA_SVC_TUNER);
        snprintf(outbuf,outlen,rc?"failed errno %d\n":"ok\n",rc?-rc:0);
        return rc? -1:0;
    }
    if(!strcmp(cmd,"epg-query")) {
        if(force || !a1 || !a2 || !a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t service=0;int64_t from=0,to=0;
        if(decimal(a1,&service) || service>UINT32_MAX ||
           signed_decimal64(a2,&from) || signed_decimal64(a3,&to) || from>=to){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_epg_iface *epg=h->api.lookup(h,OA_SVC_EPG,1);
        if(!epg){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        struct oa_epg_event events[16];size_t count=0;
        int rc=epg->query((uint32_t)service,from,to,events,16,&count);
        h->api.release(h,OA_SVC_EPG);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        struct reply r={outbuf,outlen,0};
        append(&r,"count %zu\n",count);
        for(size_t i=0;i<count && i<16;i++)
            append(&r,"event id=%u service=%u start=%" PRId64 " end=%" PRId64
                " crid=%u first=%u state=%u version=%u actual=%u running=%u"
                " name=%s\n",events[i].event_id,events[i].service_handle,
                events[i].start_ms,events[i].end_ms,events[i].program_crid_id,
                events[i].first_showing,events[i].state,events[i].version,
                events[i].actual,events[i].running_status,events[i].name);
        return 0;
    }
    if(!strcmp(cmd,"epg-ingest")) {
        if(force || !a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint8_t section[4096];size_t len=0;
        if(hex_decode(a1,section,sizeof section,&len)){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_epg_ingest_iface *ingest=
            h->api.lookup(h,OA_SVC_EPG_INGEST,1);
        if(!ingest){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=ingest->feed_section(section,len);
        h->api.release(h,OA_SVC_EPG_INGEST);
        if(rc<0){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"ok events %d\n",rc);
        return 0;
    }
    if(!strcmp(cmd,"epg-clear")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_epg_ingest_iface *ingest=
            h->api.lookup(h,OA_SVC_EPG_INGEST,1);
        if(!ingest){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        ingest->clear();
        h->api.release(h,OA_SVC_EPG_INGEST);
        snprintf(outbuf,outlen,"ok\n");
        return 0;
    }
    if(!strcmp(cmd,"epg-count")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_epg_ingest_iface *ingest=
            h->api.lookup(h,OA_SVC_EPG_INGEST,1);
        if(!ingest){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        size_t count=ingest->count();
        h->api.release(h,OA_SVC_EPG_INGEST);
        snprintf(outbuf,outlen,"count %zu\n",count);
        return 0;
    }
    if(!strcmp(cmd,"epg-nownext")) {
        if(force || !a1 || !a2 || a3){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t service=0;int64_t now=0;
        if(decimal(a1,&service) || !service || service>UINT32_MAX ||
           signed_decimal64(a2,&now) || now<0 ||
           now>(int64_t)UINT32_MAX*1000){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_epg_iface *epg=h->api.lookup(h,OA_SVC_EPG,1);
        if(!epg){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        enum { NEXT_CAP = 512 };
        struct oa_epg_event *events=calloc(NEXT_CAP,sizeof *events);
        size_t count=0;
        int rc=events?epg->query((uint32_t)service,0,
            (int64_t)UINT32_MAX*1000,events,NEXT_CAP,&count):-ENOMEM;
        h->api.release(h,OA_SVC_EPG);
        if(rc){free(events);snprintf(outbuf,outlen,"failed errno %d\n",-rc);
            return -1;}
        const struct oa_epg_event *current=NULL,*next=NULL;
        size_t copy=count<NEXT_CAP?count:NEXT_CAP;
        for(size_t i=0;i<copy;i++){
            const struct oa_epg_event *e=&events[i];
            if(e->start_ms<=now && now<e->end_ms){
                if(!current || e->start_ms<current->start_ms)current=e;
            }else if(e->start_ms>now){
                if(!next || e->start_ms<next->start_ms)next=e;
            }
        }
        struct reply r={outbuf,outlen,0};
        if(current)append(&r,"now id=%u name='%s'",current->event_id,
            current->name);
        else append(&r,"now -");
        if(next)append(&r," next id=%u name='%s'\n",next->event_id,next->name);
        else append(&r," next -\n");
        free(events);
        return 0;
    }
    if(!strcmp(cmd,"channel-ingest")) {
        if(force || !a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        char xml[8192];size_t len=0;
        if(hex_decode(a1,(uint8_t *)xml,sizeof xml - 1,&len)){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        xml[len]=0;
        const oa_channel_iface *channels=
            h->api.lookup(h,OA_SVC_CHANNELS,1);
        if(!channels){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=channels->ingest_xml(xml,len);
        h->api.release(h,OA_SVC_CHANNELS);
        if(rc<0){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"ok channels %d\n",rc);
        return 0;
    }
    if(!strcmp(cmd,"channel-clear")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_channel_iface *channels=
            h->api.lookup(h,OA_SVC_CHANNELS,1);
        if(!channels){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        channels->clear();
        h->api.release(h,OA_SVC_CHANNELS);
        snprintf(outbuf,outlen,"ok\n");
        return 0;
    }
    if(!strcmp(cmd,"channel-state")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_channel_iface *channels=
            h->api.lookup(h,OA_SVC_CHANNELS,1);
        if(!channels){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        const struct oa_channel_snapshot *snap=channels->snapshot();
        if(snap)
            snprintf(outbuf,outlen,
                "mode runtime count %zu lcn %s visibility %s\n",snap->count,
                snap->lcn_known?"known":"unknown",
                snap->visibility_known?"known":"unknown");
        else
            snprintf(outbuf,outlen,
                "mode stored count 0 lcn unknown visibility unknown\n");
        h->api.release(h,OA_SVC_CHANNELS);
        return 0;
    }
    if(!strcmp(cmd,"channel-list")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_channel_iface *channels=
            h->api.lookup(h,OA_SVC_CHANNELS,1);
        if(!channels){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        const struct oa_channel_snapshot *snap=channels->snapshot();
        struct reply r={outbuf,outlen,0};
        if(!snap){
            append(&r,"source stored count 0\n");
        }else{
            append(&r,"source runtime count %zu lcn unknown visibility unknown\n",
                snap->count);
            for(size_t i=0;i<snap->count;i++)
                append(&r,"row rsn=%u handle=%u name=%s onid=%u tsid=%u "
                    "sid=%u type=%u\n",snap->entries[i].rsn,
                    snap->entries[i].handle,snap->entries[i].name,
                    snap->entries[i].onid,snap->entries[i].tsid,
                    snap->entries[i].sid,snap->entries[i].type);
        }
        h->api.release(h,OA_SVC_CHANNELS);
        return 0;
    }
    if(!strcmp(cmd,"pvr-list")) {
        if(force || a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_pvr_iface *pvr=h->api.lookup(h,OA_SVC_PVR,2);
        if(!pvr){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        struct oa_pvr_job jobs[16];size_t count=0;
        int rc=pvr->list(jobs,16,&count);
        h->api.release(h,OA_SVC_PVR);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        struct reply r={outbuf,outlen,0};
        append(&r,"count %zu\n",count);
        for(size_t i=0;i<count && i<16;i++)
            append(&r,"job id=%u service=%u state=%u start=%" PRId64
                " end=%" PRId64 " repeat=%u pad=%d/%d name=%s\n",
                jobs[i].id,jobs[i].service_handle,jobs[i].state,
                jobs[i].start_ms,jobs[i].end_ms,jobs[i].repeat_days,
                jobs[i].start_padding,jobs[i].end_padding,jobs[i].name);
        return 0;
    }
    if(!strcmp(cmd,"pvr-add")) {
        if(force || !a1 || !a2 || !a3 || a7){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t service=0;int64_t start=0,end=0;
        if(decimal(a1,&service) || service>UINT32_MAX ||
           signed_decimal64(a2,&start) || signed_decimal64(a3,&end) || start>=end){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        struct oa_pvr_request req;
        memset(&req,0,sizeof req);
        req.service_handle=(uint32_t)service;
        req.start_ms=start;
        req.end_ms=end;
        snprintf(req.name,sizeof req.name,"cli reservation");
        if(a4){
            uint64_t repeat=0;
            if(!a5 || !a6 || decimal(a4,&repeat) || repeat>UINT32_MAX ||
               signed_decimal(a5,&req.start_padding) ||
               signed_decimal(a6,&req.end_padding)){
                snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
            req.repeat_days=(uint32_t)repeat;
        }
        const oa_pvr_iface *pvr=h->api.lookup(h,OA_SVC_PVR,2);
        if(!pvr){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        uint32_t id=0;
        int rc=pvr->add(&req,&id);
        h->api.release(h,OA_SVC_PVR);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"id %u\n",id);
        return 0;
    }
    if(!strcmp(cmd,"pvr-cancel")) {
        if(force || !a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t id=0;
        if(decimal(a1,&id) || id>UINT32_MAX){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_pvr_iface *pvr=h->api.lookup(h,OA_SVC_PVR,2);
        if(!pvr){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=pvr->cancel((uint32_t)id);
        h->api.release(h,OA_SVC_PVR);
        snprintf(outbuf,outlen,rc?"failed errno %d\n":"ok\n",rc?-rc:0);
        return rc? -1:0;
    }
    if(!strcmp(cmd,"pvr-record") || !strcmp(cmd,"pvr-stop")) {
        if(force || !a1 || a2 || a3 || a4){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t id=0;
        if(decimal(a1,&id) || id>UINT32_MAX){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_pvr_iface *pvr=h->api.lookup(h,OA_SVC_PVR,2);
        if(!pvr){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=!strcmp(cmd,"pvr-record") ? pvr->record((uint32_t)id)
                                         : pvr->finish((uint32_t)id);
        h->api.release(h,OA_SVC_PVR);
        snprintf(outbuf,outlen,rc?"failed errno %d\n":"ok\n",rc?-rc:0);
        return rc? -1:0;
    }
    if(!strcmp(cmd,"service-list")) {
        if(force){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_service_list_iface *svc=
            h->api.lookup(h,OA_SVC_SERVICE_LIST,1);
        if(!svc){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        const struct oa_service_list *list=svc->snapshot();
        if(!list){h->api.release(h,OA_SVC_SERVICE_LIST);
            snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        if(!a1) {
            size_t count=list->count;
            h->api.release(h,OA_SVC_SERVICE_LIST);
            snprintf(outbuf,outlen,"count %zu\n",count);
            return 0;
        }
        if(!strcmp(a1,"name") || !strcmp(a1,"channel")) {
            if(!a2 || a3){h->api.release(h,OA_SVC_SERVICE_LIST);
                snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
            char *end=NULL;
            errno=0;
            unsigned long long value=strtoull(a2,&end,10);
            if(errno || end==a2 || !end || *end){h->api.release(h,OA_SVC_SERVICE_LIST);
                snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
            struct oa_service_query q;
            memset(&q,0,sizeof q);
            q.source_mask=0x7;
            q.sort=(uint8_t)(!strcmp(a1,"name")?OA_SERVICE_SORT_NAME:
                OA_SERVICE_SORT_CHANNEL_NO);
            size_t *order=malloc(list->count*sizeof *order);
            size_t matched=0;
            if(!order){h->api.release(h,OA_SVC_SERVICE_LIST);
                snprintf(outbuf,outlen,"failed errno %d\n",ENOMEM);return -1;}
            int qrc=oa_service_list_query(list,&q,order,list->count,&matched);
            if(qrc || value>=matched) {
                free(order);h->api.release(h,OA_SVC_SERVICE_LIST);
                snprintf(outbuf,outlen,"failed errno %d\n",qrc?-qrc:EINVAL);
                return -1;
            }
            char line[512];
            int written=svc->format(list,order[value],line,sizeof line);
            free(order);h->api.release(h,OA_SVC_SERVICE_LIST);
            if(written<0 || (size_t)written>=sizeof line) {
                snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;
            }
            snprintf(outbuf,outlen,"%s",line);
            return 0;
        }
        if(!strcmp(a1,"stats")) {
            if(a2 || a3){h->api.release(h,OA_SVC_SERVICE_LIST);
                snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
            size_t total=list->count;
            size_t dvb=0,easi=0,analog=0,visible=0,available=0,encrypted=0;
            size_t numeric=0,missing=0;
            for(size_t i=0;i<total;i++) {
                const struct oa_service_row *row=&list->rows[i];
                if(row->source==OA_SERVICE_SOURCE_DVB)dvb++;
                else if(row->source==OA_SERVICE_SOURCE_EASI)easi++;
                else if(row->source==OA_SERVICE_SOURCE_ANALOG)analog++;
                visible+=(size_t)oa_service_visible(row);
                available+=(size_t)oa_service_available(row);
                encrypted+=(size_t)oa_service_encrypted(row);
                numeric+=(size_t)oa_service_numeric_selection(row);
                missing+=(size_t)oa_service_not_found(row);
            }
            h->api.release(h,OA_SVC_SERVICE_LIST);
            snprintf(outbuf,outlen,
                "count %zu dvb %zu easi %zu analog %zu visible %zu available %zu encrypted %zu numeric %zu missing %zu\n",
                total,dvb,easi,analog,visible,available,encrypted,
                numeric,missing);
            return 0;
        }
        if(a2 || a3){h->api.release(h,OA_SVC_SERVICE_LIST);
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        char *end=NULL;
        errno=0;
        unsigned long long value=strtoull(a1,&end,10);
        if(errno || end==a1 || !end || *end || value>=(unsigned long long)list->count) {
            h->api.release(h,OA_SVC_SERVICE_LIST);
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);
            return -1;
        }
        char line[512];
        int written=svc->format(list,(size_t)value,line,sizeof line);
        h->api.release(h,OA_SVC_SERVICE_LIST);
        if(written<0 || (size_t)written>=sizeof line) {
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);
            return -1;
        }
        snprintf(outbuf,outlen,"%s",line);
        return 0;
    }
    if(!strcmp(cmd,"launcher-items")) {
        if(force){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_launcher_db_iface *db=h->api.lookup(h,OA_SVC_LAUNCHER_DB,1);
        if(!db){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int deleted=0,outofsync=0,all=0,error=0;
        int32_t parent=0;
        if(a1 && !strcmp(a1,"all"))all=1;
        else if(a1) {
            char *end=NULL;errno=0;long value=strtol(a1,&end,10);
            if(errno || end==a1 || !end || *end || value<INT32_MIN || value>INT32_MAX) {
                h->api.release(h,OA_SVC_LAUNCHER_DB);
                snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;
            }
            parent=(int32_t)value;
        }
        char *args[2]={a2,a3};
        for(unsigned i=0;i<2;++i) {
            if(!args[i])continue;
            if(!strcmp(args[i],"deleted"))deleted=1;
            else if(!strcmp(args[i],"outofsync"))outofsync=1;
            else {error=EINVAL;break;}
        }
        struct oa_launcher_list *list=NULL;
        if(!error) {
            error=all?db->read_all(&list)
                     :db->read_parent(parent,deleted,outofsync,&list);
        }
        if(error) {
            if(list)db->release(list);
            h->api.release(h,OA_SVC_LAUNCHER_DB);
            snprintf(outbuf,outlen,"failed errno %d\n",error<0?-error:error);
            return -1;
        }
        struct reply r={outbuf,outlen,0};
        append(&r,"count %zu\n",db->count(list));
        size_t rows=db->count(list);
        for(size_t i=0;i<rows;++i) {
            char row[256];
            if(db->format(list,i,row,sizeof row)>=0)append(&r,"%s",row);
        }
        db->release(list);
        h->api.release(h,OA_SVC_LAUNCHER_DB);
        return 0;
    }
    if(!strcmp(cmd,"amp-profile")) {
        if(a1 || force){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_audio_amp_profile_iface *s=h->api.lookup(h,OA_SVC_AUDIO_AMP_PROFILE,1);
        if(!s){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        oa_ad82587_profile p;char power[32];int rc=s->copy(&p,power);
        h->api.release(h,OA_SVC_AUDIO_AMP_PROFILE);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"power '%s' prescale %" PRIu32 " ch1 %" PRIu32
            " ch2 %" PRIu32 " fine %" PRIu32 " clip %" PRIu32 " drc %" PRIu32
            " state %" PRId32 " attack %" PRId32 " release %" PRId32 "\n",
            power,p.prescale,p.channel1,p.channel2,p.fine_tune,p.power_clip,p.speaker_drc,
            p.state_control4,p.limiter_attack,p.release_threshold);
        return 0;
    }
    if(!strcmp(cmd,"amp-state")) {
        if(force || a1 || a2){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_audio_amp_iface *s=h->api.lookup(h,OA_SVC_AUDIO_AMP,1);
        if(!s){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        oa_audio_amp_status st;int rc=s->state(&st);
        h->api.release(h,OA_SVC_AUDIO_AMP);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"amp enabled %d busy %d result %d ticket %" PRIu64
            " reset %u mute %u live %d known %d balance %d balance_known %d\n",
            st.enabled,st.busy,st.native_result,st.ticket,
            (unsigned)st.reset_cached,(unsigned)st.mute_cached,st.mute,
            st.mute_known,st.balance,st.balance_known);
        return 0;
    }
    if(!strcmp(cmd,"amp-balance")) {
        if(force || !a1 || a2){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        char *end=NULL;
        long balance=strtol(a1,&end,10);
        if(!*a1 || !end || *end || balance<0 || balance>100){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_audio_amp_iface *s=h->api.lookup(h,OA_SVC_AUDIO_AMP,1);
        if(!s){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=s->set_balance?(int)s->set_balance((int)balance):-ENOTSUP;
        h->api.release(h,OA_SVC_AUDIO_AMP);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"ok\n");
        return 0;
    }
    if(!strcmp(cmd,"amp-mute")) {
        if(force || !a1 || a2){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        int on;
        if(!strcmp(a1,"0"))on=0;
        else if(!strcmp(a1,"1"))on=1;
        else {snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_audio_amp_iface *s=h->api.lookup(h,OA_SVC_AUDIO_AMP,1);
        if(!s){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=s->set_mute(on);
        h->api.release(h,OA_SVC_AUDIO_AMP);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"ok\n");
        return 0;
    }
    if(!strcmp(cmd,"amp-init")) {
        if(force || a1 || a2){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_audio_amp_iface *s=h->api.lookup(h,OA_SVC_AUDIO_AMP,1);
        if(!s){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        int rc=s->init();
        h->api.release(h,OA_SVC_AUDIO_AMP);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",-rc);return -1;}
        snprintf(outbuf,outlen,"ok\n");
        return 0;
    }
    if(!strcmp(cmd,"audio-curve") || !strcmp(cmd,"audio-curve-dump")) {
        int dump=!strcmp(cmd,"audio-curve-dump");
        if(force || (dump ? a1!=NULL : (!a1 || a2))){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_audio_curve_iface *curve=
            h->api.lookup(h,OA_SVC_AUDIO_CURVE,1);
        if(!curve){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        if(dump){
            oa_audio_curve c;
            int rc=curve->copy(&c);
            h->api.release(h,OA_SVC_AUDIO_CURVE);
            if(rc){snprintf(outbuf,outlen,"failed errno %d\n",EIO);return -1;}
            struct reply r={outbuf,outlen,0};
            append(&r,"curve %u",(unsigned)OA_AUDIO_CURVE_POINTS);
            for(unsigned i=0;i<OA_AUDIO_CURVE_POINTS;i++)
                append(&r," %u",(unsigned)c.points[i]);
            append(&r,"\n");
            return 0;
        }
        uint64_t volume=0;
        if(decimal(a1,&volume) || volume>UINT_MAX){
            h->api.release(h,OA_SVC_AUDIO_CURVE);
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint16_t value=0;
        int rc=curve->value((unsigned)volume,&value);
        h->api.release(h,OA_SVC_AUDIO_CURVE);
        if(rc){snprintf(outbuf,outlen,"failed errno %d\n",EIO);return -1;}
        snprintf(outbuf,outlen,"curve %u\n",(unsigned)value);
        return 0;
    }
    if(!strcmp(cmd,"audio-speaker")) {
        if(force || !a1 || !a2 || a3){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        uint64_t level=0;int64_t balance=0;
        if(decimal(a1,&level) || level>UINT_MAX ||
           signed_decimal64(a2,&balance) ||
           balance<INT_MIN || balance>INT_MAX){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        struct oa_audio_speaker_state state;
        if(oa_audio_speaker_map((unsigned)level,(int)balance,&state)){
            snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        snprintf(outbuf,outlen,"speaker level %u left %u right %u balance %u\n",
            state.level,state.left,state.right,state.balance);
        return 0;
    }
    if(!strcmp(cmd,"hotel-state")) {
        if(a1 || force){snprintf(outbuf,outlen,"failed errno %d\n",EINVAL);return -1;}
        const oa_settings_hotel_iface *hotel=h->api.lookup(h,OA_SVC_SETTINGS_HOTEL,1);
        if(!hotel){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        struct oa_tab_hotel state;int error=hotel->read(&state);
        h->api.release(h,OA_SVC_SETTINGS_HOTEL);
        if(error)snprintf(outbuf,outlen,"failed errno %d\n",error);
        else snprintf(outbuf,outlen,"hotel %u osd %" PRIu32 " picture %u\n",
                      (unsigned)state.mode_available,state.osd_menu_mode,(unsigned)state.picture_flag);
        return error?-1:0;
    }
    if(!strcmp(cmd,"nav-state") || !strcmp(cmd,"nav-operation") ||
       !strcmp(cmd,"nav-result") || !strcmp(cmd,"nav-take") ||
       !strcmp(cmd,"nav-set-tab") || !strcmp(cmd,"nav-set-item") || !strcmp(cmd,"nav-set-submenu")) {
        int query=!strcmp(cmd,"nav-state"),consume=!strcmp(cmd,"nav-take");
        int op=!strcmp(cmd,"nav-operation"),error=0;
        int tab=!strcmp(cmd,"nav-set-tab"),item=!strcmp(cmd,"nav-set-item");
        int submenu=!strcmp(cmd,"nav-set-submenu"),setting=tab || item || submenu;
        uint64_t value=0;int32_t result=0;
        if(a2 || force || ((query || consume)?a1!=NULL:
           (op || submenu)?(decimal(a1,&value) || value>(submenu?255:UINT32_MAX)):signed_decimal(a1,&result)))error=EINVAL;
        else if(!query && (h->shutting_down || h->stop_requested))error=ESHUTDOWN;
        if(error){snprintf(outbuf,outlen,"failed errno %d\n",error);return -1;}
        const void *service=h->api.lookup(h,OA_SVC_UI_NAVIGATION,setting?2:1);
        if(!service) {
            error=setting && oa_host_service_exists(h,OA_SVC_UI_NAVIGATION)?ENOTSUP:ENODEV;
            snprintf(outbuf,outlen,"failed errno %d\n",error);return -1;
        }
        const oa_ui_navigation_iface *nav=service;
        if(query) {
            oa_ui_navigation_state s;error=nav->state(&s);
            if(!error)snprintf(outbuf,outlen,"tab %" PRIu32 " item %" PRId32
                " submenu %" PRIu32 " pending %" PRIu32 "\n",s.tab,s.item,s.open_submenu,s.pending);
        } else if(consume) {
            oa_ui_navigation_request r;error=nav->take(&r);
            if(!error)snprintf(outbuf,outlen,"navigation %" PRIu32 " tab %" PRIu32
                " item %" PRId32 " submenu %" PRIu32 " pending %" PRIu32 "\n",
                r.navigation,r.selection.tab,r.selection.item,r.selection.open_submenu,r.selection.pending);
        } else if(setting) {
            const oa_ui_navigation_iface_v2 *v2=service;
            error=tab?v2->set_tab(result):item?v2->set_item(result):v2->set_open_submenu((uint8_t)value);
            if(!error)snprintf(outbuf,outlen,"ok\n");
        } else {
            error=op?nav->operation((uint32_t)value):nav->result(result);
            if(!error)snprintf(outbuf,outlen,"queued\n");
        }
        h->api.release(h,OA_SVC_UI_NAVIGATION);
        if(error)snprintf(outbuf,outlen,"failed errno %d\n",error);
        return error?-1:0;
    }
    if (!strcmp(cmd, "key")) {
        uint64_t code = 0;
        if (force || !a1 || a2 || decimal(a1, &code) || code > 255) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_input_iface *input = h->api.lookup(h, OA_SVC_INPUT, 1);
        if (!input) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        unsigned char frames[18];
        for (int i = 0; i < 2; i++) {
            unsigned char *f = frames + i * 9;
            f[0] = 0xff;
            f[1] = 0xff;
            f[2] = i ? 0x5e : 0x50;
            f[3] = (unsigned char)code;
            f[4] = 0x11;
            f[5] = (unsigned char)code;
            f[6] = (unsigned char)~code;
            f[7] = 0xb7;
            f[8] = 0xb8;
        }
        int rc = input->feed(frames, sizeof frames);
        h->api.release(h, OA_SVC_INPUT);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", EIO);
            return -1;
        }
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if(!strcmp(cmd,"list-state") || !strcmp(cmd,"list-key")) {
        int query=!strcmp(cmd,"list-state");
        uint64_t value=0;int error=0,handled=0;
        if(a2 || force || (query?a1!=NULL:(decimal(a1,&value) || value>UINT32_MAX)))error=EINVAL;
        else if(!query && (h->shutting_down || h->stop_requested))error=ESHUTDOWN;
        if(error){snprintf(outbuf,outlen,"failed errno %d\n",error);return -1;}
        const oa_ui_list_iface *list=h->api.lookup(h,OA_SVC_UI_LIST,1);
        if(!list){snprintf(outbuf,outlen,"failed errno %d\n",ENODEV);return -1;}
        if(query) {
            oa_ui_list_state s;error=list->state(&s);
            if(!error)snprintf(outbuf,outlen,"first %" PRIu32 " size %" PRIu32
                " selected %" PRIu32 " total %" PRIu32 " handled %d anim %" PRIu32
                " scroll %" PRIu32 " partial %" PRIu32 " focus %" PRIu32 " group %" PRIu32 "\n",
                s.first,s.size,s.selected,s.total,s.handled,s.anim,s.scroll,
                s.partial,s.focus,s.group);
        } else {
            error=list->key((uint32_t)value,&handled);
            if(!error)snprintf(outbuf,outlen,"handled %d\n",handled);
        }
        h->api.release(h,OA_SVC_UI_LIST);
        if(error)snprintf(outbuf,outlen,"failed errno %d\n",error);
        return error?-1:0;
    }
    if (!strcmp(cmd, "cec-state")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_cec_service *owner =
            h->api.lookup(h, OA_SVC_CEC_OWNER, 1);
        const oa_cec_audio_iface *audio =
            h->api.lookup(h, OA_SVC_CEC_AUDIO, 1);
        if (!owner || !audio) {
            if (owner) h->api.release(h, OA_SVC_CEC_OWNER);
            if (audio) h->api.release(h, OA_SVC_CEC_AUDIO);
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int supported = owner->hardware_open_supported ?
            owner->hardware_open_supported() : 0;
        oa_cec_transport_status transport;
        memset(&transport, 0, sizeof transport);
        if (owner->status) (void)owner->status(&transport);
        oa_cec_audio_route route;
        memset(&route, 0, sizeof route);
        if (audio->route) audio->route(&route);
        h->api.release(h, OA_SVC_CEC_OWNER);
        h->api.release(h, OA_SVC_CEC_AUDIO);
        snprintf(outbuf, outlen,
                 "cec owner 1 audio 1 supported %d local %d enabled %d "
                 "opening %d open %d rx %" PRIu64 " tx %" PRIu64
                 " error %d last %d\n",
                 supported, route.local_volume_permitted, transport.enabled,
                 transport.opening, transport.open, transport.rx_frames,
                 transport.tx_frames, transport.last_error,
                 transport.have_last_rx);
        return 0;
    }
    if (!strcmp(cmd, "cec-open") || !strcmp(cmd, "cec-close")) {
        if (force || a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_cec_service *owner =
            h->api.lookup(h, OA_SVC_CEC_OWNER, 1);
        if (!owner) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int open = !strcmp(cmd, "cec-open");
        int rc = open ? (owner->open ? owner->open() : -ENOTSUP)
                      : (owner->close ? owner->close() : -ENOTSUP);
        h->api.release(h, OA_SVC_CEC_OWNER);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", -rc);
            return -1;
        }
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if (!strcmp(cmd, "cec-send")) {
        if (force || !a1 || a2) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        size_t length = strlen(a1);
        if (!length || (length & 1u) || length > OA_CEC_MAX_FRAME * 2u) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        uint8_t wire[OA_CEC_MAX_FRAME];
        for (size_t i = 0; i < length / 2; i++) {
            int hi = a1[2 * i], lo = a1[2 * i + 1];
            hi = hi >= '0' && hi <= '9' ? hi - '0'
               : hi >= 'a' && hi <= 'f' ? hi - 'a' + 10
               : hi >= 'A' && hi <= 'F' ? hi - 'A' + 10 : -1;
            lo = lo >= '0' && lo <= '9' ? lo - '0'
               : lo >= 'a' && lo <= 'f' ? lo - 'a' + 10
               : lo >= 'A' && lo <= 'F' ? lo - 'A' + 10 : -1;
            if (hi < 0 || lo < 0) {
                snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
                return -1;
            }
            wire[i] = (uint8_t)((hi << 4) | lo);
        }
        oa_cec_message message;
        if (oa_cec_decode(&message, wire, length / 2) != OA_CEC_VALID) {
            snprintf(outbuf, outlen, "failed errno %d\n", EINVAL);
            return -1;
        }
        const oa_cec_service *owner =
            h->api.lookup(h, OA_SVC_CEC_OWNER, 1);
        if (!owner) {
            snprintf(outbuf, outlen, "failed errno %d\n", ENODEV);
            return -1;
        }
        int rc = owner->send ? owner->send(&message) : -ENOTSUP;
        h->api.release(h, OA_SVC_CEC_OWNER);
        if (rc) {
            snprintf(outbuf, outlen, "failed errno %d\n", -rc);
            return -1;
        }
        snprintf(outbuf, outlen, "ok\n");
        return 0;
    }
    if (!strcmp(cmd, "mixer-query") || !strcmp(cmd, "mixer-result") || !strcmp(cmd, "mixer-cancel")) {
        uint64_t value;
        int query = !strcmp(cmd, "mixer-query");
        int error = 0, rc = -1;
        if (a2 || decimal(a1, &value) || (query ? value > 5 : value == 0)) error = EINVAL;
        else if (query && (h->shutting_down || h->stop_requested)) error = ESHUTDOWN;
        if (error) { snprintf(outbuf, outlen, "failed errno %d\n", error); return -1; }
        const oa_audio_mixer_iface *mixer = h->api.lookup(h, OA_SVC_AUDIO_MIXER, 1);
        if (!mixer) { snprintf(outbuf, outlen, "failed errno %d\n", ENODEV); return -1; }
        if (query) {
            uint64_t ticket;
            rc = mixer->gain((unsigned)value, &ticket);
            error = errno;
            if (!rc) snprintf(outbuf, outlen, "ticket %" PRIu64 "\n", ticket);
        } else if (!strcmp(cmd, "mixer-result")) {
            oa_audio_broker_reply reply;
            rc = mixer->take(value, &reply);
            error = errno;
            if (rc == 0) snprintf(outbuf, outlen, "pending %" PRIu64 "\n", value);
            else if (rc == 1) snprintf(outbuf, outlen,
                "result %" PRIu64 " error %d device %u left %u right %u\n",
                reply.ticket, reply.error, reply.gain.device, reply.gain.left, reply.gain.right);
        } else {
            rc = mixer->cancel(value);
            error = errno;
            if (!rc) snprintf(outbuf, outlen, "ok\n");
        }
        h->api.release(h, OA_SVC_AUDIO_MIXER);
        if (rc < 0) snprintf(outbuf, outlen, "failed errno %d\n", error);
        return rc < 0 ? -1 : 0;
    }
    if (!strcmp(cmd, "list")) {
        cmd_list(h, outbuf, outlen);
        return 0;
    }
    if (!strcmp(cmd, "status") && a1) {
        cmd_status(h, a1, outbuf, outlen);
        return 0;
    }
    if (!strcmp(cmd, "load") && a1) {
        oa_mod *m = oa_module_load(h, a1);
        snprintf(outbuf, outlen, "%s %s\n", m ? "ok" : "failed", a1);
        return m ? 0 : -1;
    }
    int bop = -1;
    if (!strcmp(cmd, "load-batch")) bop = BATCH_LOAD;
    else if (!strcmp(cmd, "start-batch")) bop = BATCH_START;
    else if (!strcmp(cmd, "stop-batch")) bop = BATCH_STOP;
    else if (!strcmp(cmd, "unload-batch")) bop = BATCH_UNLOAD;
    else if (!strcmp(cmd, "reload-batch")) bop = BATCH_RELOAD;
    if (bop >= 0) {
        if (!a1) {
            snprintf(outbuf, outlen, "usage: %s <name> [name...]\n", cmd);
            return -1;
        }
        int queued = 0, dropped = 0;
        const char *head[7] = { a1, a2, a3, a4, a5, a6, a7 };
        for (int i = 0; i < 7 && head[i]; i++) {
            if (batch_enqueue(head[i], bop))
                queued++;
            else
                dropped++;
        }
        char *more = NULL;
        while (save && (more = strtok_r(NULL, " \t\r\n", &save)) != NULL) {
            if (batch_enqueue(more, bop))
                queued++;
            else
                dropped++;
        }
        if (dropped)
            snprintf(outbuf, outlen, "queued %d dropped %d\n", queued, dropped);
        else
            snprintf(outbuf, outlen, "queued %d\n", queued);
        return 0;
    }
    if (!strcmp(cmd, "start") && a1) {
        oa_mod *m = oa_host_find_module(h, a1);
        int rc = m ? oa_module_start(h, m) : -1;
        snprintf(outbuf, outlen, "%s\n", rc == 0 ? "ok" : "failed");
        return rc;
    }
    if (!strcmp(cmd, "stop") && a1) {
        oa_mod *m = oa_host_find_module(h, a1);
        if (!m) {
            snprintf(outbuf, outlen, "no such module\n");
            return -1;
        }
        int rc = oa_module_stop(h, m);
        snprintf(outbuf, outlen, "%s\n", rc == 0 ? "ok" : "failed");
        return rc;
    }
    if (!strcmp(cmd, "unload") && a1) {
        int rc = oa_module_unload(h, a1, force);
        snprintf(outbuf, outlen, "%s\n", rc == 0 ? "ok" : "failed");
        return rc;
    }
    if (!strcmp(cmd, "reload") && a1) {
        int rc = oa_module_reload(h, a1, force);
        snprintf(outbuf, outlen, "%s\n", rc == 0 ? "ok" : "failed");
        return rc;
    }
    if (!strcmp(cmd, "suspend")) {
        int rc = oa_module_suspend_all(h);
        snprintf(outbuf, outlen, rc == 0 ? "ok\n" : "suspend failed\n");
        return rc;
    }
    if (!strcmp(cmd, "standby")) {
        const oa_power_iface *power = h->api.lookup(h, OA_SVC_POWER, 1);
        int rc = power ? power->standby() : -1;
        snprintf(outbuf, outlen, rc == 0 ? "ok\n" : "standby failed\n");
        return rc;
    }
    if (!strcmp(cmd, "resume")) {
        int rc = oa_module_resume_all(h);
        snprintf(outbuf, outlen, rc == 0 ? "ok\n" : "resume failed\n");
        return rc;
    }
    if (!strcmp(cmd, "loglevel") && a1) {
        h->log_level = atoi(a1);
        snprintf(outbuf, outlen, "loglevel %d\n", h->log_level);
        return 0;
    }
    if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit")) {
        snprintf(outbuf, outlen, "bye\n");
        return 1;
    }
    snprintf(outbuf, outlen, "unknown command: %s (try 'help')\n", cmd);
    return -1;
}

/* -2 needs more input; otherwise the command result. Discard an entire bad
 * line, never execute the tail of an overlong command as another command. */
static int next_command(oa_host *h, struct input *in, char *reply)
{
    char *end = memchr(in->buf, '\n', in->len);
    if (!end) {
        if (in->len == sizeof in->buf) {
            in->discard = 1;
            in->len = 0;
        }
        return -2;
    }
    size_t len = (size_t)(end - in->buf);
    int rc;
    if (in->discard || memchr(in->buf, 0, len)) {
        snprintf(reply, CONTROL_REPLY_MAX, "error: command too long or contains NUL\n");
        rc = -1;
    } else {
        *end = 0;
        rc = oa_control_command(h, in->buf, reply, CONTROL_REPLY_MAX);
    }
    memmove(in->buf, end + 1, in->len - len - 1);
    in->len -= len + 1;
    in->discard = 0;
    return rc;
}

/* ------------------------------------------------------------------ io */

static void on_client(void *ud, int fd, unsigned rev);

static void client_close(oa_host *h, struct client *c)
{
    if (c->watch) h->api.watch_del(h, c->watch);
    if (c->fd >= 0) close(c->fd);
    memset(c, 0, sizeof *c);
    c->fd = -1;
}

static int client_flush(oa_host *h, struct client *c)
{
    while (c->sent < c->pending) {
        ssize_t n = send(c->fd, c->output + c->sent, c->pending - c->sent,
                         MSG_DONTWAIT | MSG_NOSIGNAL);
        if (n > 0) c->sent += (size_t)n;
        else if (n < 0 && errno == EINTR) continue;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        else {
            client_close(h, c);
            return -1;
        }
    }
    c->sent = c->pending = 0;
    return 0;
}

static size_t client_space(const struct client *c)
{
    return sizeof c->output - (c->pending - c->sent);
}

static void client_commands(oa_host *h, struct client *c)
{
    while (c->fd >= 0 && client_space(c) >= CONTROL_REPLY_MAX) {
        char reply[CONTROL_REPLY_MAX];
        int rc = next_command(h, &c->input, reply);
        if (rc == -2) break;
        if (c->sent) {
            memmove(c->output, c->output + c->sent, c->pending - c->sent);
            c->pending -= c->sent;
            c->sent = 0;
        }
        size_t len = strlen(reply);
        memcpy(c->output + c->pending, reply, len);
        c->pending += len;
        int failed = client_flush(h, c) != 0;
        if (rc == 1) {
            /* Shutdown is immediate; a blocked reader cannot defer quit. */
            oa_host_quit(h);
            return;
        }
        if (failed) return;
    }
}

static void client_watch(oa_host *h, struct client *c)
{
    unsigned events = (!c->eof && client_space(c) >= CONTROL_REPLY_MAX) ? OA_WATCH_READ : 0;
    if (c->pending > c->sent) events |= OA_WATCH_WRITE;
    if (!events) {
        client_close(h, c);
        return;
    }
    if (c->watch && c->events == events) return;
    if (c->watch) h->api.watch_del(h, c->watch);
    c->events = events;
    c->watch = h->api.watch_add(h, c->fd, events, on_client, h);
    if (!c->watch) client_close(h, c);
}

static void on_accept(void *ud, int fd, unsigned rev)
{
    (void)rev;
    oa_host *h = ud;
    int c = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (c < 0)
        return;
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_ctl.clients[i].fd < 0) {
            g_ctl.clients[i].fd = c;
            client_watch(h, &g_ctl.clients[i]);
            return;
        }
    close(c);
}

static void on_client(void *ud, int fd, unsigned rev)
{
    oa_host *h = ud;
    struct client *c = NULL;
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_ctl.clients[i].fd == fd) {
            c = &g_ctl.clients[i];
            break;
        }
    if (!c)
        return;
    if (rev & OA_WATCH_ERR) {
        client_close(h, c);
        return;
    }
    if (client_flush(h, c) != 0) return;
    client_commands(h, c);
    if (c->fd < 0 || !h->running) return;
    if ((rev & OA_WATCH_READ) && !c->eof && client_space(c) >= CONTROL_REPLY_MAX) {
        struct input *in = &c->input;
        ssize_t n = read(fd, in->buf + in->len, sizeof in->buf - in->len);
        if (n > 0) in->len += (size_t)n;
        else if (n == 0) c->eof = 1;
        else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            client_close(h, c);
            return;
        }
        client_commands(h, c);
    }
    if (c->fd >= 0 && h->running) client_watch(h, c);
}

static void on_stdin(void *ud, int fd, unsigned rev)
{
    (void)rev;
    oa_host *h = ud;
    struct input *in = &g_ctl.stdin_input;
    ssize_t n = read(fd, in->buf + in->len, sizeof in->buf - in->len);
    if (n <= 0) {
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) return;
        /* EOF: stop watching stdin, keep the process alive */
        if (g_ctl.stdin_watch) {
            h->api.watch_del(h, g_ctl.stdin_watch);
            g_ctl.stdin_watch = 0;
        }
        return;
    }
    in->len += (size_t)n;
    char reply[CONTROL_REPLY_MAX];
    int rc;
    while ((rc = next_command(h, in, reply)) != -2) {
        if (*reply) out(h, 1, "%s", reply);
        if (rc == 1) {
            oa_host_quit(h);
            break;
        }
    }
}

/* Only remove a refused, still-identical stale socket. A second core must not
 * unlink a live listener or a caller's regular file. */
static int prepare_path(const struct sockaddr_un *sa)
{
    struct stat before, after;
    if (lstat(sa->sun_path, &before) != 0) return errno == ENOENT ? 0 : -1;
    if (!S_ISSOCK(before.st_mode)) return -1;
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return -1;
    int rc = connect(fd, (const struct sockaddr *)sa, sizeof *sa);
    int err = errno;
    close(fd);
    if (rc == 0 || err != ECONNREFUSED) return -1;
    if (lstat(sa->sun_path, &after) != 0 ||
        before.st_dev != after.st_dev || before.st_ino != after.st_ino) return -1;
    return unlink(sa->sun_path);
}

int oa_control_start(oa_host *h, const char *sock_path)
{
    if (g_ctl.owner) return -1;
    memset(&g_ctl, 0, sizeof g_ctl);
    g_batch_head = g_batch_count = 0;
    g_ctl.owner = h;
    g_ctl.listen_fd = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
        g_ctl.clients[i].fd = -1;

    if (sock_path && *sock_path) {
        struct sockaddr_un sa;
        memset(&sa, 0, sizeof sa);
        sa.sun_family = AF_UNIX;
        if (strlen(sock_path) >= sizeof sa.sun_path) goto fail;
        strcpy(sa.sun_path, sock_path);
        if (prepare_path(&sa) != 0) goto fail;
        struct stat st;
        g_ctl.listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (g_ctl.listen_fd < 0 || bind(g_ctl.listen_fd, (struct sockaddr *)&sa, sizeof sa) != 0) {
            h->api.log(h, OA_LOG_WARN, "control socket %s: %s", sock_path, strerror(errno));
            goto fail;
        }
        if (lstat(sock_path, &st) != 0) goto fail;
        strcpy(g_ctl.path, sock_path);
        g_ctl.dev = st.st_dev;
        g_ctl.ino = st.st_ino;
        if (listen(g_ctl.listen_fd, 4) != 0) goto fail;
        g_ctl.listen_watch = h->api.watch_add(h, g_ctl.listen_fd, OA_WATCH_READ, on_accept, h);
        if (!g_ctl.listen_watch) goto fail;
        h->api.log(h, OA_LOG_INFO, "control socket %s", sock_path);
    }

    g_ctl.stdin_watch = h->api.watch_add(h, 0, OA_WATCH_READ, on_stdin, h);
    if (!g_ctl.stdin_watch) goto fail;
    return 0;
fail:
    oa_control_stop(h);
    return -1;
}

void oa_control_stop(oa_host *h)
{
    if (g_ctl.owner != h) return;
    g_batch_head = g_batch_count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
        client_close(h, &g_ctl.clients[i]);
    if (g_ctl.stdin_watch) h->api.watch_del(h, g_ctl.stdin_watch);
    if (g_ctl.listen_watch) h->api.watch_del(h, g_ctl.listen_watch);
    if (g_ctl.listen_fd >= 0) {
        close(g_ctl.listen_fd);
        g_ctl.listen_fd = -1;
    }
    struct stat st;
    if (*g_ctl.path && lstat(g_ctl.path, &st) == 0 &&
        st.st_dev == g_ctl.dev && st.st_ino == g_ctl.ino)
        unlink(g_ctl.path);
    memset(&g_ctl, 0, sizeof g_ctl);
    g_ctl.listen_fd = -1;
}
