#include "openaurora/lua_skin.h"
#include "openaurora/module.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

extern char **environ;

#define LAYOUTS 16
#define BINDS 16
#define ANIMS 8
#define STATES 32
#define TIMERS 4
#define ACTION_MAX 32
#define STATE_KEY 32
#define STATE_STR 64
#define SCRIPT_MAX (48 * 1024)
#define FETCHES 2
#define FETCH_BODY (64 * 1024)
#define URL_MAX 480

struct box {
    size_t used;
    size_t limit;
    uint32_t steps;
    uint32_t max_steps;
    uint32_t interval;
};

struct layout {
    char name[OA_LUA_NAME_MAX];
    int x, y, w, h;
    int used;
};

struct bind {
    int key;
    char action[ACTION_MAX];
    int used;
};

struct anim {
    char id[OA_LUA_NAME_MAX];
    int duration;
    int easing;
    int used;
};

struct model {
    struct layout layouts[LAYOUTS];
    struct bind binds[BINDS];
    struct anim anims[ANIMS];
    int nlayout, nbind, nanim;
    int has_nav, nav_band, nav_index;
    int has_theme;
    int bg[4], accent[4];
};

struct slot {
    uint64_t id;
    int ref;
    uint32_t generation;
    struct session *session;
    struct runtime *rt;
};

struct runtime;

#define CONTROL_SLOTS 4
struct control_slot {
    struct runtime *rt;
    uint32_t id;
    int used;
    char text[192];
};

struct session;

struct fetch {
    int used;
    int ref;
    uint32_t generation;
    char url[URL_MAX];
    pid_t pid;
    int fd;
    uint32_t watch;
    char *body;
    size_t len;
    int eof;
    int ready;
    int signaled;
    char err[96];
    struct session *session;
    struct runtime *rt;
};

struct session {
    struct box box;
    struct model model;
    lua_State *L;
    char *source;
    size_t source_len;
    struct slot slots[TIMERS];
    struct fetch fetches[FETCHES];
};

struct runtime {
    const oa_host_api *api;
    char path[256];
    char data_dir[256];
    size_t memory_limit;
    uint32_t max_steps;
    size_t max_bytes;
    struct session *live;
    int error;
    int suspended;
    int in_call;
    int from_blob;
    int published;
    int quiescing;
    uint32_t generation;
    uint32_t timer_gen;
    uint32_t fetch_gen;
    uint64_t pump_id;
    char message[160];
    unsigned char *snap;
    size_t snap_len;
    uint32_t owner;
    int control_mode;
    uint32_t control_seq;
    struct control_slot control_slots[CONTROL_SLOTS];
    struct {
        int valid;
        uint32_t id;
        int rc;
        char reply[512];
    } control_last;
};

static struct runtime *g_rt;
static char model_key;
static char rt_key;
static char session_key;
static char io_open_key;

static void *alloc_lua(void *ud, void *ptr, size_t osize, size_t nsize)
{
    struct box *b = ud;
    size_t old = ptr ? osize : 0;
    if (nsize == 0) {
        free(ptr);
        if (b->used >= old)
            b->used -= old;
        else
            b->used = 0;
        return NULL;
    }
    if (old > b->used)
        old = b->used;
    if (nsize > b->limit || b->used - old > b->limit - nsize)
        return NULL;
    void *next = realloc(ptr, nsize);
    if (!next)
        return NULL;
    b->used = b->used - old + nsize;
    return next;
}

static void hook(lua_State *L, lua_Debug *ar)
{
    void *ud = NULL;
    struct box *b;
    (void)ar;
    lua_getallocf(L, &ud);
    b = ud;
    if (!b)
        luaL_error(L, "instruction limit");
    if (b->steps >= b->max_steps || b->max_steps - b->steps < b->interval)
        b->steps = b->max_steps;
    else
        b->steps += b->interval;
    if (b->steps >= b->max_steps)
        luaL_error(L, "instruction limit");
}

static void set_note(struct runtime *rt, int err, const char *msg)
{
    rt->error = err;
    snprintf(rt->message, sizeof rt->message, "%s", msg ? msg : "");
    if (rt->api && msg && msg[0])
        rt->api->log(rt->api->priv, OA_LOG_ERROR, "ui.lua: %s", msg);
}

static int map_msg(struct runtime *rt, const char *msg)
{
    int err = EIO;
    if (!msg)
        msg = "lua error";
    if (strstr(msg, "instruction limit"))
        err = EAGAIN;
    else if (strstr(msg, "not enough memory") || strstr(msg, "memory"))
        err = ENOMEM;
    else if (strstr(msg, "syntax") || strstr(msg, "unexpected") ||
             strstr(msg, "'<eof>'") || strstr(msg, "oa_"))
        err = EINVAL;
    set_note(rt, err, msg);
    return -1;
}

static struct model *model_of(lua_State *L)
{
    struct model *m;
    lua_pushlightuserdata(L, &model_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    m = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return m;
}

static struct runtime *rt_of(lua_State *L)
{
    struct runtime *rt;
    lua_pushlightuserdata(L, &rt_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    rt = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return rt;
}

static int range_int(lua_State *L, int idx, lua_Integer lo, lua_Integer hi, int *out)
{
    int ok = 0;
    lua_Integer v = lua_tointegerx(L, idx, &ok);
    if (!ok || v < lo || v > hi)
        return -1;
    *out = (int)v;
    return 0;
}

static int copy_text(lua_State *L, int idx, char *dst, size_t cap)
{
    size_t n = 0;
    const char *s = lua_tolstring(L, idx, &n);
    if (lua_type(L, idx) != LUA_TSTRING || !s || n == 0 || n >= cap)
        return -1;
    memcpy(dst, s, n);
    dst[n] = 0;
    return 0;
}

static int l_layout(lua_State *L)
{
    struct model *m = model_of(L);
    struct layout rec;
    int i;
    memset(&rec, 0, sizeof rec);
    if (!m)
        return luaL_error(L, "no model");
    if (copy_text(L, 1, rec.name, sizeof rec.name) != 0 ||
        range_int(L, 2, -8192, 8192, &rec.x) != 0 ||
        range_int(L, 3, -8192, 8192, &rec.y) != 0 ||
        range_int(L, 4, 1, 8192, &rec.w) != 0 ||
        range_int(L, 5, 1, 8192, &rec.h) != 0)
        return luaL_error(L, "layout arguments");
    rec.used = 1;
    for (i = 0; i < m->nlayout; i++)
        if (!strcmp(m->layouts[i].name, rec.name)) {
            m->layouts[i] = rec;
            return 0;
        }
    if (m->nlayout >= LAYOUTS)
        return luaL_error(L, "layout table full");
    m->layouts[m->nlayout++] = rec;
    return 0;
}

static int l_nav(lua_State *L)
{
    struct model *m = model_of(L);
    int band = 0, index = 0;
    if (!m)
        return luaL_error(L, "no model");
    if (range_int(L, 1, 0, 15, &band) != 0 || range_int(L, 2, 0, 255, &index) != 0)
        return luaL_error(L, "nav arguments");
    m->has_nav = 1;
    m->nav_band = band;
    m->nav_index = index;
    return 0;
}

static int l_theme(lua_State *L)
{
    struct model *m = model_of(L);
    int i, ch[8];
    if (!m)
        return luaL_error(L, "no model");
    for (i = 0; i < 8; i++)
        if (range_int(L, i + 1, 0, 255, &ch[i]) != 0)
            return luaL_error(L, "theme arguments");
    for (i = 0; i < 4; i++) {
        m->bg[i] = ch[i];
        m->accent[i] = ch[i + 4];
    }
    m->has_theme = 1;
    return 0;
}

static int l_bind(lua_State *L)
{
    struct model *m = model_of(L);
    struct bind rec;
    int i;
    memset(&rec, 0, sizeof rec);
    if (!m)
        return luaL_error(L, "no model");
    if (range_int(L, 1, 0, 255, &rec.key) != 0 ||
        copy_text(L, 2, rec.action, sizeof rec.action) != 0)
        return luaL_error(L, "bind arguments");
    rec.used = 1;
    for (i = 0; i < m->nbind; i++)
        if (m->binds[i].key == rec.key) {
            m->binds[i] = rec;
            return 0;
        }
    if (m->nbind >= BINDS)
        return luaL_error(L, "binding table full");
    m->binds[m->nbind++] = rec;
    return 0;
}

static int l_anim(lua_State *L)
{
    struct model *m = model_of(L);
    struct anim rec;
    const char *easing;
    int i;
    memset(&rec, 0, sizeof rec);
    if (!m)
        return luaL_error(L, "no model");
    easing = lua_tostring(L, 3);
    if (copy_text(L, 1, rec.id, sizeof rec.id) != 0 ||
        range_int(L, 2, 1, 600000, &rec.duration) != 0 ||
        lua_type(L, 3) != LUA_TSTRING || !easing)
        return luaL_error(L, "anim arguments");
    if (!strcmp(easing, "linear"))
        rec.easing = 0;
    else if (!strcmp(easing, "cubic"))
        rec.easing = 1;
    else
        return luaL_error(L, "anim easing");
    rec.used = 1;
    for (i = 0; i < m->nanim; i++)
        if (!strcmp(m->anims[i].id, rec.id)) {
            m->anims[i] = rec;
            return 0;
        }
    if (m->nanim >= ANIMS)
        return luaL_error(L, "anim table full");
    m->anims[m->nanim++] = rec;
    return 0;
}

static int l_nav_band(lua_State *L)
{
    struct model *m = model_of(L);
    lua_pushinteger(L, m && m->has_nav ? m->nav_band : -1);
    return 1;
}

static int l_nav_index(lua_State *L)
{
    struct model *m = model_of(L);
    lua_pushinteger(L, m && m->has_nav ? m->nav_index : -1);
    return 1;
}

static int l_bound(lua_State *L)
{
    struct model *m = model_of(L);
    int key = 0, i;
    if (!m || range_int(L, 1, 0, 255, &key) != 0)
        return luaL_error(L, "bound arguments");
    for (i = 0; i < m->nbind; i++)
        if (m->binds[i].used && m->binds[i].key == key) {
            lua_pushstring(L, m->binds[i].action);
            return 1;
        }
    lua_pushnil(L);
    return 1;
}

static int l_anim_ms(lua_State *L)
{
    struct model *m = model_of(L);
    char id[OA_LUA_NAME_MAX];
    int i;
    if (!m || copy_text(L, 1, id, sizeof id) != 0)
        return luaL_error(L, "anim_ms arguments");
    for (i = 0; i < m->nanim; i++)
        if (m->anims[i].used && !strcmp(m->anims[i].id, id)) {
            lua_pushinteger(L, m->anims[i].duration);
            return 1;
        }
    lua_pushnil(L);
    return 1;
}

static void drop_slot(struct runtime *rt, struct session *s, struct slot *slot)
{
    if (slot->id && rt->api)
        rt->api->timer_del(rt->api->priv, slot->id);
    slot->id = 0;
    if (s && s->L && slot->ref != LUA_NOREF)
        luaL_unref(s->L, LUA_REGISTRYINDEX, slot->ref);
    slot->ref = LUA_NOREF;
    slot->session = NULL;
}

static void drop_timers(struct runtime *rt, struct session *s)
{
    int i;
    rt->timer_gen++;
    if (!s)
        return;
    for (i = 0; i < TIMERS; i++)
        drop_slot(rt, s, &s->slots[i]);
}

static void on_timer(void *ud)
{
    struct slot *slot = ud;
    struct runtime *rt = slot->rt;
    struct session *s = slot->session;
    int ref = slot->ref;
    slot->id = 0;
    if (!rt || !s || !s->L || slot->generation != rt->timer_gen || rt->suspended) {
        slot->ref = LUA_NOREF;
        if (s && s->L && ref != LUA_NOREF)
            luaL_unref(s->L, LUA_REGISTRYINDEX, ref);
        return;
    }
    slot->ref = LUA_NOREF;
    lua_rawgeti(s->L, LUA_REGISTRYINDEX, ref);
    luaL_unref(s->L, LUA_REGISTRYINDEX, ref);
    s->box.steps = 0;
    lua_sethook(s->L, hook, LUA_MASKCOUNT, s->box.interval);
    if (lua_pcall(s->L, 0, 0, 0) != 0) {
        const char *msg = lua_tostring(s->L, -1);
        map_msg(rt, msg);
        lua_pop(s->L, 1);
    }
    lua_sethook(s->L, NULL, 0, 0);
}

static int l_after(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    struct session *s;
    int ms = 0, i, ref;
    if (!rt || !rt->live || rt->live->L != L)
        return luaL_error(L, "timers are not available during validation");
    s = rt->live;
    if (range_int(L, 1, 0, 60000, &ms) != 0)
        return luaL_error(L, "timer delay");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    for (i = 0; i < TIMERS; i++)
        if (!s->slots[i].id)
            break;
    if (i == TIMERS)
        return luaL_error(L, "timer table full");
    lua_pushvalue(L, 2);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    s->slots[i].ref = ref;
    s->slots[i].generation = rt->timer_gen;
    s->slots[i].session = s;
    s->slots[i].rt = rt;
    s->slots[i].id = rt->api->timer_add(rt->api->priv, (uint32_t)ms, 0, on_timer, &s->slots[i]);
    if (!s->slots[i].id) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        s->slots[i].ref = LUA_NOREF;
        return luaL_error(L, "timer rejected");
    }
    return 0;
}

static struct session *session_of(lua_State *L)
{
    struct session *s;
    lua_pushlightuserdata(L, &session_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    s = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return s;
}

static int mode_writes(const char *mode)
{
    const char *p;
    if (!mode)
        return 0;
    for (p = mode; *p; p++)
        if (*p == 'w' || *p == 'a' || *p == '+')
            return 1;
    return 0;
}

static int under_data(const struct runtime *rt, const char *path)
{
    size_t n;
    if (!rt || !rt->data_dir[0] || !path)
        return 0;
    n = strlen(rt->data_dir);
    if (strncmp(path, rt->data_dir, n) != 0 || path[n] != '/' || path[n + 1] == 0)
        return 0;
    if (strstr(path, ".."))
        return 0;
    return 1;
}

static int call_real_open(lua_State *L, const char *path, const char *mode)
{
    lua_pushlightuserdata(L, &io_open_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushstring(L, path);
    lua_pushstring(L, mode ? mode : "r");
    lua_call(L, 2, 2);
    return 2;
}

/* Reads must be regular files so /proc stats work and devices do not.
 * Writes stay under ui.lua.data. The original io.open still reports ENOENT. */
static int l_io_open(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    size_t pn = 0, mn = 0;
    const char *path = luaL_checklstring(L, 1, &pn);
    const char *mode = luaL_optlstring(L, 2, "r", &mn);
    struct stat st;
    int fd;
    if (!path || pn != strlen(path) || pn == 0 || pn > 4000 ||
        !mode || mn != strlen(mode) || mn == 0 || mn > 8)
        return luaL_error(L, "io.open arguments");
    if (mode_writes(mode)) {
        if (!under_data(rt, path)) {
            lua_pushnil(L);
            lua_pushstring(L, "read only");
            return 2;
        }
        return call_real_open(L, path, mode);
    }
    fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return call_real_open(L, path, mode);
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        lua_pushnil(L);
        lua_pushstring(L, "not a regular file");
        return 2;
    }
    close(fd);
    return call_real_open(L, path, mode);
}

static int url_ok(const char *url, size_t n)
{
    size_t i;
    if (!url || n != strlen(url) || n < 8 || n >= URL_MAX)
        return 0;
    if (strncmp(url, "https://", 8) != 0 && strncmp(url, "http://", 7) != 0)
        return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)url[i];
        if (c <= 32 || c >= 127)
            return 0;
    }
    return 1;
}

static void close_fetch_io(struct runtime *rt, struct fetch *f)
{
    if (f->watch && rt && rt->api) {
        rt->api->watch_del(rt->api->priv, f->watch);
        f->watch = 0;
    }
    if (f->fd >= 0) {
        close(f->fd);
        f->fd = -1;
    }
}

static void reap_fetch(struct fetch *f, int block)
{
    int st = 0;
    pid_t got;
    if (f->pid <= 0)
        return;
    kill(f->pid, block ? SIGKILL : SIGTERM);
    do {
        got = waitpid(f->pid, &st, block ? 0 : WNOHANG);
    } while (got < 0 && errno == EINTR);
    if (got == f->pid || (got < 0 && errno == ECHILD))
        f->pid = 0;
}

static void clear_fetch(struct fetch *f)
{
    free(f->body);
    memset(f, 0, sizeof *f);
    f->fd = -1;
    f->ref = LUA_NOREF;
}

static void cancel_session_fetches(struct runtime *rt, struct session *s)
{
    int i;
    if (!rt || !s)
        return;
    for (i = 0; i < FETCHES; i++) {
        struct fetch *f = &s->fetches[i];
        if (!f->used && f->pid <= 0 && f->fd < 0)
            continue;
        reap_fetch(f, 1);
        close_fetch_io(rt, f);
        if (s->L && f->ref != LUA_NOREF)
            luaL_unref(s->L, LUA_REGISTRYINDEX, f->ref);
        clear_fetch(f);
    }
}

static int spawn_curl(const char *url, int *fd_out, pid_t *pid_out)
{
    int pipes[2];
    posix_spawn_file_actions_t fa;
    pid_t pid = 0;
    char *argv[] = {
        "curl", "--proto", "=http,https", "--proto-redir", "=http,https",
        "--max-time", "5", "--max-filesize", "65536", "-sS",
        "--output", "-", "--", (char *)url, NULL
    };
    int rc;
    /* O_NONBLOCK at creation: fcntl() is GLIBC_2.28 in this toolchain and the TV is 2.23. */
    if (pipe2(pipes, O_CLOEXEC | O_NONBLOCK) != 0)
        return -1;
    if (posix_spawn_file_actions_init(&fa) != 0) {
        close(pipes[0]);
        close(pipes[1]);
        return -1;
    }
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&fa, pipes[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&fa, pipes[0]);
    posix_spawn_file_actions_addclose(&fa, pipes[1]);
    rc = posix_spawn(&pid, "/usr/bin/curl", &fa, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(pipes[1]);
    if (rc != 0) {
        close(pipes[0]);
        errno = rc;
        return -1;
    }
    *fd_out = pipes[0];
    *pid_out = pid;
    return 0;
}

static void deliver_fetch(struct runtime *rt, struct session *s, struct fetch *f)
{
    struct model saved;
    int args;
    if (!f->used || f->ready == 2)
        return;
    if (!s || !s->L || f->generation != rt->fetch_gen || rt->suspended || rt->quiescing ||
        s != rt->live) {
        if (s && s->L && f->ref != LUA_NOREF && f->generation == rt->fetch_gen)
            luaL_unref(s->L, LUA_REGISTRYINDEX, f->ref);
        clear_fetch(f);
        return;
    }
    lua_rawgeti(s->L, LUA_REGISTRYINDEX, f->ref);
    luaL_unref(s->L, LUA_REGISTRYINDEX, f->ref);
    f->ref = LUA_NOREF;
    if (f->err[0]) {
        lua_pushnil(s->L);
        lua_pushstring(s->L, f->err);
    } else if (f->len > FETCH_BODY || s->box.used > s->box.limit ||
               s->box.limit - s->box.used < f->len + 256) {
        lua_pushnil(s->L);
        lua_pushstring(s->L, "not enough memory");
    } else {
        lua_pushlstring(s->L, f->body ? f->body : "", f->len);
        lua_pushnil(s->L);
    }
    args = 2;
    saved = s->model;
    s->box.steps = 0;
    lua_sethook(s->L, hook, LUA_MASKCOUNT, s->box.interval);
    if (lua_pcall(s->L, args, 0, 0) != 0) {
        s->model = saved;
        map_msg(rt, lua_tostring(s->L, -1));
        lua_pop(s->L, 1);
    }
    lua_sethook(s->L, NULL, 0, 0);
    free(f->body);
    f->body = NULL;
    f->used = 0;
    f->ready = 2;
    f->ref = LUA_NOREF;
}

static void finish_fetch(struct runtime *rt, struct session *s, struct fetch *f)
{
    int st = 0;
    pid_t got;
    if (!f->used || f->ready || !f->eof)
        return;
    if (f->pid > 0) {
        got = waitpid(f->pid, &st, WNOHANG);
        if (got == 0 || (got < 0 && errno == EINTR))
            return;
        if (got > 0 && !(WIFEXITED(st) && WEXITSTATUS(st) == 0) && !f->err[0])
            snprintf(f->err, sizeof f->err, "fetch failed");
        f->pid = 0;
    }
    close_fetch_io(rt, f);
    f->ready = 1;
    if (s)
        deliver_fetch(rt, s, f);
}

static void on_fetch_watch(void *ud, int fd, unsigned revents)
{
    struct fetch *f = ud;
    char buf[4096];
    (void)revents;
    if (!f || f->fd < 0 || f->fd != fd)
        return;
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            snprintf(f->err, sizeof f->err, "fetch read");
            f->eof = 1;
            break;
        }
        if (n == 0) {
            f->eof = 1;
            break;
        }
        if (f->len + (size_t)n > FETCH_BODY) {
            snprintf(f->err, sizeof f->err, "response too large");
            f->eof = 1;
            if (f->pid > 0)
                kill(f->pid, SIGTERM);
            break;
        }
        {
            char *next = realloc(f->body, f->len + (size_t)n);
            if (!next) {
                snprintf(f->err, sizeof f->err, "not enough memory");
                f->eof = 1;
                if (f->pid > 0)
                    kill(f->pid, SIGTERM);
                break;
            }
            f->body = next;
            memcpy(f->body + f->len, buf, (size_t)n);
            f->len += (size_t)n;
        }
    }
    if (f->eof && f->rt)
        finish_fetch(f->rt, f->rt->live, f);
}

static void on_pump(void *ud)
{
    struct runtime *rt = ud;
    struct session *s;
    int i;
    if (!rt || rt->quiescing || rt->suspended)
        return;
    s = rt->live;
    if (!s)
        return;
    for (i = 0; i < FETCHES; i++) {
        struct fetch *f = &s->fetches[i];
        if (!f->used || f->ready == 2)
            continue;
        if (f->pid <= 0 && !f->eof && !f->err[0]) {
            int fd = -1;
            pid_t pid = 0;
            if (access("/usr/bin/curl", X_OK) != 0 || spawn_curl(f->url, &fd, &pid) != 0) {
                snprintf(f->err, sizeof f->err, access("/usr/bin/curl", X_OK) != 0 ?
                         "curl missing" : "cannot start curl");
                f->eof = 1;
            } else {
                f->fd = fd;
                f->pid = pid;
                f->watch = rt->api->watch_add(rt->api->priv, fd, OA_WATCH_READ, on_fetch_watch, f);
                if (!f->watch) {
                    reap_fetch(f, 1);
                    close_fetch_io(rt, f);
                    snprintf(f->err, sizeof f->err, "fetch watch");
                    f->eof = 1;
                }
            }
        }
        if (f->pid <= 0 && f->err[0] && !f->eof)
            f->eof = 1;
        if (f->eof && !f->ready)
            finish_fetch(rt, s, f);
    }
}

static int l_fetch(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    struct session *s = session_of(L);
    size_t n = 0;
    const char *url = luaL_checklstring(L, 1, &n);
    int i;
    struct fetch *f;
    if (!rt || !s)
        return luaL_error(L, "no runtime");
    /* A candidate may queue a fetch. The pump starts it only after that
     * session is installed as live, and a rejected candidate cancels it. */
    if (rt->quiescing || rt->suspended)
        return luaL_error(L, "fetch is not available");
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (!url_ok(url, n))
        return luaL_error(L, "fetch url");
    for (i = 0; i < FETCHES; i++)
        if (!s->fetches[i].used)
            break;
    if (i == FETCHES)
        return luaL_error(L, "fetch table full");
    f = &s->fetches[i];
    memset(f, 0, sizeof *f);
    f->fd = -1;
    f->ref = LUA_NOREF;
    f->used = 1;
    f->generation = rt->fetch_gen;
    f->session = s;
    f->rt = rt;
    memcpy(f->url, url, n);
    lua_pushvalue(L, 2);
    f->ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

static const luaL_Reg oa_funcs[] = {
    {"layout", l_layout},
    {"nav", l_nav},
    {"theme", l_theme},
    {"bind", l_bind},
    {"anim", l_anim},
    {"nav_band", l_nav_band},
    {"nav_index", l_nav_index},
    {"bound", l_bound},
    {"anim_ms", l_anim_ms},
    {"after", l_after},
    {"fetch", l_fetch},
    {NULL, NULL}
};

static int control_text_ok(const char *text, size_t len)
{
    if (!text || len == 0 || len >= 192)
        return 0;
    for (size_t i = 0; i < len; i++)
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126)
            return 0;
    return 1;
}

static void next_word(const char **cursor, char *out, size_t cap)
{
    const char *p = *cursor;
    size_t n = 0;
    while (*p == ' ' || *p == '\t') p++;
    while (*p && *p != ' ' && *p != '\t' && n + 1 < cap)
        out[n++] = *p++;
    out[n] = 0;
    while (*p && *p != ' ' && *p != '\t') p++;
    *cursor = p;
}

static int query_allowed(const char *text)
{
    static const char *allowed[] = {
        "list", "status", "help", "service-list", "channel-list",
        "channel-state", "epg-count", "epg-query", "tuner-state",
        "pvr-list", "ui-state", "nav-state", "list-state", "list-key",
        "mixer-query", "mixer-result", "hotel-state", "launcher-items",
        NULL
    };
    const char *p = text;
    char first[32];
    next_word(&p, first, sizeof first);
    for (int i = 0; allowed[i]; i++)
        if (!strcmp(first, allowed[i]))
            return 1;
    return 0;
}

static int control_forbidden(const char *text)
{
    const char *p = text;
    char first[32], second[32];
    next_word(&p, first, sizeof first);
    if (!strcmp(first, "exit") || !strcmp(first, "quit") ||
        !strcmp(first, "standby"))
        return 1;
    if (!strcmp(first, "load") || !strcmp(first, "start") ||
        !strcmp(first, "stop") || !strcmp(first, "unload") ||
        !strcmp(first, "reload") || !strcmp(first, "suspend") ||
        !strcmp(first, "resume")) {
        next_word(&p, second, sizeof second);
        if (!strcmp(second, "--force"))
            next_word(&p, second, sizeof second);
        if (!strcmp(second, "lua"))
            return 1;
        if ((!strcmp(first, "load") || !strcmp(first, "reload")) &&
            (strchr(second, '/') || strstr(second, "..") ||
             strstr(second, ".so")))
            return 1;
    }
    return 0;
}

static void run_control(void *ud)
{
    struct control_slot *slot = ud;
    struct runtime *rt = slot ? slot->rt : NULL;
    char reply[512];
    if (!rt || !rt->api || !rt->api->control) {
        if (slot) slot->used = 0;
        return;
    }
    memset(reply, 0, sizeof reply);
    int rc = rt->api->control(rt->api->priv, slot->text, reply, sizeof reply);
    rt->control_last.valid = 1;
    rt->control_last.id = slot->id;
    rt->control_last.rc = rc;
    snprintf(rt->control_last.reply, sizeof rt->control_last.reply, "%s", reply);
    slot->used = 0;
}

static int l_aurora_query(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    size_t len = 0;
    const char *text = luaL_checklstring(L, 1, &len);
    if (!rt || !rt->live || rt->live->L != L)
        return luaL_error(L, "aurora query is not available during validation");
    if (rt->control_mode < 1 || !rt->api->control)
        return luaL_error(L, "aurora control is disabled");
    if (!control_text_ok(text, len))
        return luaL_error(L, "aurora command length or characters");
    if (!query_allowed(text))
        return luaL_error(L, "not a read-only aurora command");
    char reply[512];
    memset(reply, 0, sizeof reply);
    int rc = rt->api->control(rt->api->priv, text, reply, sizeof reply);
    lua_pushstring(L, reply);
    lua_pushboolean(L, rc == 0);
    return 2;
}

static int l_aurora_request(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    size_t len = 0;
    const char *text = luaL_checklstring(L, 1, &len);
    if (!rt || !rt->live || rt->live->L != L)
        return luaL_error(L, "aurora request is not available during validation");
    if (rt->control_mode < 2 || !rt->api->control || !rt->api->post)
        return luaL_error(L, "aurora control is disabled");
    if (!control_text_ok(text, len))
        return luaL_error(L, "aurora command length or characters");
    if (control_forbidden(text))
        return luaL_error(L, "aurora command is not permitted");
    if (!rt->owner)
        return luaL_error(L, "aurora control owner unavailable");
    for (int i = 0; i < CONTROL_SLOTS; i++) {
        struct control_slot *slot = &rt->control_slots[i];
        if (slot->used)
            continue;
        memset(slot, 0, sizeof *slot);
        slot->rt = rt;
        slot->id = ++rt->control_seq;
        slot->used = 1;
        memcpy(slot->text, text, len);
        slot->text[len] = 0;
        if (rt->api->post(rt->api->priv, rt->owner, run_control, slot) != 0) {
            slot->used = 0;
            return luaL_error(L, "aurora control queue is full");
        }
        lua_pushinteger(L, slot->id);
        return 1;
    }
    return luaL_error(L, "aurora control queue is full");
}

static int l_aurora_result(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    if (!rt || !rt->control_last.valid) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushinteger(L, rt->control_last.id);
    lua_setfield(L, -2, "id");
    lua_pushboolean(L, rt->control_last.rc == 0);
    lua_setfield(L, -2, "ok");
    lua_pushstring(L, rt->control_last.reply);
    lua_setfield(L, -2, "reply");
    rt->control_last.valid = 0;
    return 1;
}

static int l_aurora_mode(lua_State *L)
{
    struct runtime *rt = rt_of(L);
    lua_pushinteger(L, rt ? rt->control_mode : 0);
    return 1;
}

static const luaL_Reg aurora_funcs[] = {
    {"query", l_aurora_query},
    {"request", l_aurora_request},
    {"result", l_aurora_result},
    {"mode", l_aurora_mode},
    {NULL, NULL}
};

static void nil_field(lua_State *L, const char *table, const char *field)
{
    lua_getglobal(L, table);
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        lua_setfield(L, -2, field);
    }
    lua_pop(L, 1);
}

static void open_safe(lua_State *L)
{
    static const char *hide[] = {
        "dofile", "loadfile", "load", "require", "debug", "package",
        "coroutine", NULL
    };
    static const char *os_hide[] = {
        "execute", "exit", "remove", "rename", "tmpname", NULL
    };
    static const char *io_hide[] = {
        "popen", "tmpfile", "stdin", "stdout", "stderr", "input", "output",
        "read", "write", "lines", "flush", "close", NULL
    };
    int i;
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_IOLIBNAME, luaopen_io, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_OSLIBNAME, luaopen_os, 1);
    lua_pop(L, 1);
    for (i = 0; hide[i]; i++) {
        lua_pushnil(L);
        lua_setglobal(L, hide[i]);
    }
    for (i = 0; os_hide[i]; i++)
        nil_field(L, "os", os_hide[i]);
    for (i = 0; io_hide[i]; i++)
        nil_field(L, "io", io_hide[i]);
    lua_getglobal(L, "io");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "open");
        lua_pushlightuserdata(L, &io_open_key);
        lua_pushvalue(L, -2);
        lua_rawset(L, LUA_REGISTRYINDEX);
        lua_pop(L, 1);
        lua_pushcfunction(L, l_io_open);
        lua_setfield(L, -2, "open");
    }
    lua_pop(L, 1);
}

static int install_api(lua_State *L, struct session *s, struct runtime *rt)
{
    lua_pushlightuserdata(L, &model_key);
    lua_pushlightuserdata(L, &s->model);
    lua_rawset(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, &rt_key);
    lua_pushlightuserdata(L, rt);
    lua_rawset(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, &session_key);
    lua_pushlightuserdata(L, s);
    lua_rawset(L, LUA_REGISTRYINDEX);
    lua_newtable(L);
    luaL_setfuncs(L, oa_funcs, 0);
    lua_pushinteger(L, OA_LUA_API_VERSION);
    lua_setfield(L, -2, "api");
    lua_newtable(L);
    lua_setfield(L, -2, "state");
    lua_newtable(L);
    luaL_setfuncs(L, aurora_funcs, 0);
    lua_pushinteger(L, rt ? rt->control_mode : 0);
    lua_setfield(L, -2, "mode");
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "aurora");
    lua_setglobal(L, "aurora");
    lua_setglobal(L, "oa");
    return 0;
}

static int protected(struct runtime *rt, struct session *s, int nargs, int nres)
{
    int rc;
    const char *msg;
    s->box.steps = 0;
    lua_sethook(s->L, hook, LUA_MASKCOUNT, s->box.interval);
    rc = lua_pcall(s->L, nargs, nres, 0);
    lua_sethook(s->L, NULL, 0, 0);
    if (rc == 0)
        return 0;
    msg = lua_tostring(s->L, -1);
    map_msg(rt, msg);
    lua_pop(s->L, 1);
    return -1;
}

static void close_session(struct runtime *rt, struct session *s)
{
    int i;
    if (!s)
        return;
    if (rt)
        cancel_session_fetches(rt, s);
    if (rt)
        rt->timer_gen++;
    for (i = 0; i < TIMERS; i++) {
        if (s->slots[i].id && rt && rt->api)
            rt->api->timer_del(rt->api->priv, s->slots[i].id);
        s->slots[i].id = 0;
        s->slots[i].ref = LUA_NOREF;
    }
    if (s->L)
        lua_close(s->L);
    free(s->source);
    free(s);
}

static struct session *new_session(struct runtime *rt)
{
    struct session *s = calloc(1, sizeof *s);
    int i;
    if (!s)
        return NULL;
    s->box.limit = rt->memory_limit;
    s->box.max_steps = rt->max_steps;
    s->box.interval = rt->max_steps < 1000 ? rt->max_steps : 1000;
    if (s->box.interval == 0)
        s->box.interval = 1;
    for (i = 0; i < TIMERS; i++)
        s->slots[i].ref = LUA_NOREF;
    for (i = 0; i < FETCHES; i++) {
        s->fetches[i].fd = -1;
        s->fetches[i].ref = LUA_NOREF;
    }
    s->L = lua_newstate(alloc_lua, &s->box);
    if (!s->L) {
        free(s);
        return NULL;
    }
    open_safe(s->L);
    install_api(s->L, s, rt);
    return s;
}

static int check_script(struct runtime *rt, struct session *s)
{
    lua_getglobal(s->L, "oa");
    if (!lua_istable(s->L, -1)) {
        lua_pop(s->L, 1);
        set_note(rt, EINVAL, "oa table replaced");
        return -1;
    }
    lua_getfield(s->L, -1, "api");
    if (lua_tointeger(s->L, -1) != OA_LUA_API_VERSION) {
        lua_pop(s->L, 2);
        set_note(rt, EINVAL, "oa.api is not 1");
        return -1;
    }
    lua_pop(s->L, 2);
    lua_getglobal(s->L, "oa_api");
    if (lua_isfunction(s->L, -1)) {
        if (protected(rt, s, 0, 1) != 0)
            return -1;
        if (lua_tointeger(s->L, -1) != OA_LUA_API_VERSION) {
            lua_pop(s->L, 1);
            set_note(rt, EINVAL, "oa_api is not 1");
            return -1;
        }
        lua_pop(s->L, 1);
    } else {
        lua_pop(s->L, 1);
    }
    return 0;
}

static int run_source(struct runtime *rt, struct session *s, const char *src, size_t len)
{
    if (luaL_loadbufferx(s->L, src, len, "ui.lua", "t") != 0) {
        map_msg(rt, lua_tostring(s->L, -1));
        lua_pop(s->L, 1);
        rt->error = EINVAL;
        return -1;
    }
    if (protected(rt, s, 0, 0) != 0)
        return -1;
    return check_script(rt, s);
}

static char *read_file(struct runtime *rt, const char *path, size_t *len)
{
    struct stat st;
    int fd;
    char *buf;
    size_t got = 0;
    if (!path || !path[0] || strlen(path) >= sizeof rt->path) {
        set_note(rt, EINVAL, "script path");
        return NULL;
    }
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        set_note(rt, errno ? errno : EIO, "cannot open script");
        return NULL;
    }
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        close(fd);
        set_note(rt, EINVAL, "script is not a regular file");
        return NULL;
    }
    if ((size_t)st.st_size == 0 || (size_t)st.st_size > rt->max_bytes) {
        close(fd);
        set_note(rt, EINVAL, st.st_size == 0 ? "script is empty" : "script too large");
        return NULL;
    }
    buf = malloc((size_t)st.st_size);
    if (!buf) {
        close(fd);
        set_note(rt, ENOMEM, "out of memory");
        return NULL;
    }
    while (got < (size_t)st.st_size) {
        ssize_t n = read(fd, buf + got, (size_t)st.st_size - got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            free(buf);
            close(fd);
            set_note(rt, errno ? errno : EIO, "cannot read script");
            return NULL;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    close(fd);
    if (got != (size_t)st.st_size) {
        free(buf);
        set_note(rt, EIO, "short script read");
        return NULL;
    }
    *len = got;
    return buf;
}

static struct session *compile_path(struct runtime *rt, const char *path)
{
    size_t len = 0;
    char *src = read_file(rt, path, &len);
    struct session *s;
    if (!src)
        return NULL;
    s = new_session(rt);
    if (!s) {
        free(src);
        set_note(rt, ENOMEM, "not enough memory");
        return NULL;
    }
    if (run_source(rt, s, src, len) != 0) {
        close_session(rt, s);
        free(src);
        return NULL;
    }
    s->source = src;
    s->source_len = len;
    return s;
}

/* --- state blob -------------------------------------------------------- */

struct cursor {
    unsigned char *p;
    size_t n, cap;
    int overflow;
};

static void put_bytes(struct cursor *c, const void *src, size_t n)
{
    if (c->overflow || n > c->cap - c->n) {
        c->overflow = 1;
        if (c->n <= SIZE_MAX - n)
            c->n += n;
        return;
    }
    if (c->p && src)
        memcpy(c->p + c->n, src, n);
    c->n += n;
}

static void put_u8(struct cursor *c, unsigned v) { unsigned char b = (unsigned char)v; put_bytes(c, &b, 1); }
static void put_u16(struct cursor *c, unsigned v)
{
    unsigned char b[2] = {(unsigned char)v, (unsigned char)(v >> 8)};
    put_bytes(c, b, 2);
}
static void put_u32(struct cursor *c, uint32_t v)
{
    unsigned char b[4] = {
        (unsigned char)v, (unsigned char)(v >> 8),
        (unsigned char)(v >> 16), (unsigned char)(v >> 24)
    };
    put_bytes(c, b, 4);
}
static void put_u64(struct cursor *c, uint64_t v)
{
    unsigned char b[8];
    int i;
    for (i = 0; i < 8; i++)
        b[i] = (unsigned char)(v >> (8 * i));
    put_bytes(c, b, 8);
}

struct reader {
    const unsigned char *p;
    size_t n, i;
};

static int get_bytes(struct reader *r, void *dst, size_t n)
{
    if (n > r->n - r->i)
        return -1;
    if (dst)
        memcpy(dst, r->p + r->i, n);
    r->i += n;
    return 0;
}
static int get_u8(struct reader *r, unsigned *v)
{
    unsigned char b;
    if (get_bytes(r, &b, 1) != 0)
        return -1;
    *v = b;
    return 0;
}
static int get_u16(struct reader *r, unsigned *v)
{
    unsigned char b[2];
    if (get_bytes(r, b, 2) != 0)
        return -1;
    *v = (unsigned)b[0] | ((unsigned)b[1] << 8);
    return 0;
}
static int get_u32(struct reader *r, uint32_t *v)
{
    unsigned char b[4];
    if (get_bytes(r, b, 4) != 0)
        return -1;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}
static int get_u64(struct reader *r, uint64_t *v)
{
    unsigned char b[8];
    int i;
    uint64_t x = 0;
    if (get_bytes(r, b, 8) != 0)
        return -1;
    for (i = 0; i < 8; i++)
        x |= (uint64_t)b[i] << (8 * i);
    *v = x;
    return 0;
}

struct state_item {
    char key[STATE_KEY + 1];
    int type; /* 1 bool, 2 int, 3 string */
    int bool_value;
    int64_t integer;
    char str[STATE_STR + 1];
    unsigned str_len;
};

static int read_state_items(struct runtime *rt, lua_State *L, struct state_item *items, int *count)
{
    int n = 0;
    *count = 0;
    if (!L)
        return 0;
    lua_getglobal(L, "oa");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        set_note(rt, EINVAL, "oa table missing");
        return -1;
    }
    lua_getfield(L, -1, "state");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        set_note(rt, EINVAL, "oa.state is not a table");
        return -1;
    }
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        size_t kn = 0, vn = 0;
        const char *key, *str;
        struct state_item *it;
        if (n >= STATES) {
            lua_pop(L, 4);
            set_note(rt, EINVAL, "oa.state is too large");
            return -1;
        }
        if (lua_type(L, -2) != LUA_TSTRING) {
            lua_pop(L, 4);
            set_note(rt, EINVAL, "oa.state key is not a string");
            return -1;
        }
        key = lua_tolstring(L, -2, &kn);
        if (!key || kn == 0 || kn > STATE_KEY) {
            lua_pop(L, 4);
            set_note(rt, EINVAL, "oa.state key length");
            return -1;
        }
        it = &items[n];
        memset(it, 0, sizeof *it);
        memcpy(it->key, key, kn);
        if (lua_type(L, -1) == LUA_TBOOLEAN) {
            it->type = 1;
            it->bool_value = lua_toboolean(L, -1) ? 1 : 0;
        } else if (lua_isinteger(L, -1)) {
            it->type = 2;
            it->integer = (int64_t)lua_tointeger(L, -1);
        } else if (lua_type(L, -1) == LUA_TSTRING) {
            str = lua_tolstring(L, -1, &vn);
            if (!str || vn > STATE_STR) {
                lua_pop(L, 4);
                set_note(rt, EINVAL, "oa.state string is too long");
                return -1;
            }
            it->type = 3;
            it->str_len = (unsigned)vn;
            memcpy(it->str, str, vn);
        } else {
            lua_pop(L, 4);
            set_note(rt, EINVAL, "oa.state value must be bool, integer, or string");
            return -1;
        }
        lua_pop(L, 1);
        n++;
    }
    lua_pop(L, 2);
    *count = n;
    return 0;
}

static void write_items(struct cursor *c, const struct state_item *items, int n)
{
    int i;
    put_u16(c, (unsigned)n);
    for (i = 0; i < n; i++) {
        size_t kn = strlen(items[i].key);
        put_u8(c, (unsigned)kn);
        put_bytes(c, items[i].key, kn);
        put_u8(c, (unsigned)items[i].type);
        if (items[i].type == 1)
            put_u8(c, items[i].bool_value ? 1 : 0);
        else if (items[i].type == 2)
            put_u64(c, (uint64_t)items[i].integer);
        else {
            put_u16(c, items[i].str_len);
            put_bytes(c, items[i].str, items[i].str_len);
        }
    }
}

static int apply_items(struct session *s, const struct state_item *items, int n)
{
    int i;
    lua_getglobal(s->L, "oa");
    if (!lua_istable(s->L, -1)) {
        lua_pop(s->L, 1);
        return -1;
    }
    lua_newtable(s->L);
    for (i = 0; i < n; i++) {
        lua_pushstring(s->L, items[i].key);
        if (items[i].type == 1)
            lua_pushboolean(s->L, items[i].bool_value);
        else if (items[i].type == 2)
            lua_pushinteger(s->L, (lua_Integer)items[i].integer);
        else
            lua_pushlstring(s->L, items[i].str, items[i].str_len);
        lua_settable(s->L, -3);
    }
    lua_setfield(s->L, -2, "state");
    lua_pop(s->L, 1);
    return 0;
}

static int migrate(struct runtime *rt, struct session *s)
{
    lua_getglobal(s->L, "oa_migrate");
    if (!lua_isfunction(s->L, -1)) {
        lua_pop(s->L, 1);
        return 0;
    }
    lua_getglobal(s->L, "oa");
    if (!lua_istable(s->L, -1)) {
        lua_pop(s->L, 2);
        set_note(rt, EINVAL, "oa table missing");
        return -1;
    }
    lua_getfield(s->L, -1, "state");
    lua_remove(s->L, -2);
    if (protected(rt, s, 1, 1) != 0)
        return -1;
    if (lua_isnil(s->L, -1)) {
        lua_pop(s->L, 1);
        return 0;
    }
    if (!lua_istable(s->L, -1)) {
        lua_pop(s->L, 1);
        set_note(rt, EINVAL, "oa_migrate must return a table or nil");
        return -1;
    }
    lua_getglobal(s->L, "oa");
    if (!lua_istable(s->L, -1)) {
        lua_pop(s->L, 2);
        set_note(rt, EINVAL, "oa table missing");
        return -1;
    }
    lua_pushvalue(s->L, -2);
    lua_setfield(s->L, -2, "state");
    lua_pop(s->L, 2);
    return 0;
}

static void write_model(struct cursor *c, const struct model *m)
{
    int i;
    put_u16(c, (unsigned)m->nlayout);
    for (i = 0; i < m->nlayout; i++) {
        put_bytes(c, m->layouts[i].name, OA_LUA_NAME_MAX);
        put_u32(c, (uint32_t)m->layouts[i].x);
        put_u32(c, (uint32_t)m->layouts[i].y);
        put_u32(c, (uint32_t)m->layouts[i].w);
        put_u32(c, (uint32_t)m->layouts[i].h);
    }
    put_u16(c, (unsigned)m->nbind);
    for (i = 0; i < m->nbind; i++) {
        put_u32(c, (uint32_t)m->binds[i].key);
        put_bytes(c, m->binds[i].action, ACTION_MAX);
    }
    put_u16(c, (unsigned)m->nanim);
    for (i = 0; i < m->nanim; i++) {
        put_bytes(c, m->anims[i].id, OA_LUA_NAME_MAX);
        put_u32(c, (uint32_t)m->anims[i].duration);
        put_u8(c, (unsigned)m->anims[i].easing);
        put_u8(c, 0);
        put_u8(c, 0);
        put_u8(c, 0);
    }
    put_u8(c, m->has_nav ? 1 : 0);
    put_u8(c, (unsigned)m->nav_band);
    put_u8(c, (unsigned)m->nav_index);
    put_u8(c, m->has_theme ? 1 : 0);
    for (i = 0; i < 4; i++)
        put_u8(c, (unsigned)m->bg[i]);
    for (i = 0; i < 4; i++)
        put_u8(c, (unsigned)m->accent[i]);
}

static int read_model(struct reader *r, struct model *m)
{
    unsigned n = 0, i, flag = 0;
    uint32_t u = 0;
    memset(m, 0, sizeof *m);
    if (get_u16(r, &n) != 0 || n > LAYOUTS)
        return -1;
    m->nlayout = (int)n;
    for (i = 0; i < n; i++) {
        uint32_t x, y, w, h;
        if (get_bytes(r, m->layouts[i].name, OA_LUA_NAME_MAX) != 0)
            return -1;
        m->layouts[i].name[OA_LUA_NAME_MAX - 1] = 0;
        if (get_u32(r, &x) || get_u32(r, &y) || get_u32(r, &w) || get_u32(r, &h))
            return -1;
        m->layouts[i].x = (int)x;
        m->layouts[i].y = (int)y;
        m->layouts[i].w = (int)w;
        m->layouts[i].h = (int)h;
        m->layouts[i].used = 1;
    }
    if (get_u16(r, &n) != 0 || n > BINDS)
        return -1;
    m->nbind = (int)n;
    for (i = 0; i < n; i++) {
        if (get_u32(r, &u) != 0 || get_bytes(r, m->binds[i].action, ACTION_MAX) != 0)
            return -1;
        m->binds[i].key = (int)u;
        m->binds[i].action[ACTION_MAX - 1] = 0;
        m->binds[i].used = 1;
    }
    if (get_u16(r, &n) != 0 || n > ANIMS)
        return -1;
    m->nanim = (int)n;
    for (i = 0; i < n; i++) {
        unsigned easing = 0, pad = 0;
        if (get_bytes(r, m->anims[i].id, OA_LUA_NAME_MAX) != 0 || get_u32(r, &u) != 0 ||
            get_u8(r, &easing) != 0 || get_u8(r, &pad) != 0 || get_u8(r, &pad) != 0 ||
            get_u8(r, &pad) != 0)
            return -1;
        m->anims[i].id[OA_LUA_NAME_MAX - 1] = 0;
        m->anims[i].duration = (int)u;
        m->anims[i].easing = (int)easing;
        m->anims[i].used = 1;
    }
    if (get_u8(r, &flag) != 0)
        return -1;
    m->has_nav = flag ? 1 : 0;
    if (get_u8(r, &flag) != 0)
        return -1;
    m->nav_band = (int)flag;
    if (get_u8(r, &flag) != 0)
        return -1;
    m->nav_index = (int)flag;
    if (get_u8(r, &flag) != 0)
        return -1;
    m->has_theme = flag ? 1 : 0;
    for (i = 0; i < 4; i++) {
        if (get_u8(r, &flag) != 0)
            return -1;
        m->bg[i] = (int)flag;
    }
    for (i = 0; i < 4; i++) {
        if (get_u8(r, &flag) != 0)
            return -1;
        m->accent[i] = (int)flag;
    }
    return 0;
}

static int read_items(struct reader *r, struct state_item *items, int *count)
{
    unsigned n = 0, i, kn = 0, typ = 0, b = 0, sl = 0;
    if (get_u16(r, &n) != 0 || n > STATES)
        return -1;
    for (i = 0; i < n; i++) {
        struct state_item *it = &items[i];
        uint64_t iv = 0;
        memset(it, 0, sizeof *it);
        if (get_u8(r, &kn) != 0 || kn == 0 || kn > STATE_KEY || get_bytes(r, it->key, kn) != 0)
            return -1;
        it->key[kn] = 0;
        if (get_u8(r, &typ) != 0)
            return -1;
        it->type = (int)typ;
        if (typ == 1) {
            if (get_u8(r, &b) != 0)
                return -1;
            it->bool_value = b ? 1 : 0;
        } else if (typ == 2) {
            if (get_u64(r, &iv) != 0)
                return -1;
            it->integer = (int64_t)iv;
        } else if (typ == 3) {
            if (get_u16(r, &sl) != 0 || sl > STATE_STR || get_bytes(r, it->str, sl) != 0)
                return -1;
            it->str_len = sl;
        } else {
            return -1;
        }
    }
    *count = (int)n;
    return 0;
}

static int adopt(struct runtime *rt, struct session *s, const struct model *saved_model,
                 const struct state_item *items, int nitems)
{
    if (saved_model)
        s->model = *saved_model;
    /* The registry still points at s->model; the struct was overwritten in place. */
    if (apply_items(s, items, nitems) != 0) {
        set_note(rt, EINVAL, "cannot restore oa.state");
        return -1;
    }
    if (migrate(rt, s) != 0)
        return -1;
    return 0;
}

static int swap_session(struct runtime *rt, struct session *s)
{
    drop_timers(rt, rt->live);
    close_session(rt, rt->live);
    rt->live = s;
    rt->generation++;
    rt->error = 0;
    rt->message[0] = 0;
    rt->from_blob = 0;
    return 0;
}

static unsigned clamp_u(const char *text, unsigned def, unsigned lo, unsigned hi)
{
    char *end = NULL;
    unsigned long v;
    if (!text || !text[0])
        return def;
    errno = 0;
    v = strtoul(text, &end, 10);
    if (errno || !end || *end || v < lo || v > hi)
        return def;
    return (unsigned)v;
}

static const oa_lua_iface g_iface;

static void drop_pump(struct runtime *rt)
{
    if (rt->pump_id && rt->api)
        rt->api->timer_del(rt->api->priv, rt->pump_id);
    rt->pump_id = 0;
}

/* The 50 ms pump is what starts a queued fetch. A published service with no
 * pump looks healthy and never runs curl, so allocation failure is fatal. */
static int arm_pump(struct runtime *rt)
{
    if (!rt || !rt->api || rt->quiescing)
        return -1;
    if (rt->pump_id)
        return 0;
    rt->pump_id = rt->api->timer_add(rt->api->priv, 50, 50, on_pump, rt);
    if (rt->pump_id)
        return 0;
    rt->api->log(rt->api->priv, OA_LOG_ERROR, "lua pump timer allocation failed");
    return -1;
}

static int publish_service(struct runtime *rt)
{
    if (rt->published)
        return 0;
    if (rt->api->publish(rt->api->priv, OA_SVC_UI_LUA, OA_LUA_API_VERSION, &g_iface) != 0)
        return -1;
    rt->published = 1;
    return 0;
}

static void unpublish_service(struct runtime *rt)
{
    if (!rt->published || !rt->api)
        return;
    rt->api->unpublish(rt->api->priv, OA_SVC_UI_LUA);
    rt->published = 0;
}

static int rt_quiesce(struct runtime *rt)
{
    int i, busy = 0;
    if (!rt)
        return 0;
    rt->quiescing = 1;
    /* Drop the service before the child is reaped so a later host turn
     * cannot look it up and swap or hook the session under the child. */
    unpublish_service(rt);
    drop_pump(rt);
    if (!rt->live)
        return 0;
    for (i = 0; i < FETCHES; i++) {
        struct fetch *f = &rt->live->fetches[i];
        int st = 0;
        pid_t got;
        if (f->pid <= 0)
            continue;
        kill(f->pid, f->signaled ? SIGKILL : SIGTERM);
        f->signaled = 1;
        got = waitpid(f->pid, &st, WNOHANG);
        if (got < 0 && errno == EINTR)
            got = waitpid(f->pid, &st, WNOHANG);
        if (got == f->pid || (got < 0 && errno == ECHILD)) {
            f->pid = 0;
            close_fetch_io(rt, f);
        } else {
            busy = 1;
        }
    }
    return busy ? -1 : 0;
}

static struct runtime *rt_create(const oa_host_api *api)
{
    struct runtime *rt;
    const char *path, *mem, *steps, *bytes, *data;
    size_t n;
    if (!api || g_rt)
        return NULL;
    rt = calloc(1, sizeof *rt);
    if (!rt)
        return NULL;
    rt->api = api;
    rt->owner = api->owner_id(api->priv);
    const char *control = api->config_get(api->priv, "ui.lua.control", "0");
    rt->control_mode = (int)clamp_u(control, 0, 0, 2);
    path = api->config_get(api->priv, "ui.lua.script", "");
    snprintf(rt->path, sizeof rt->path, "%s", path ? path : "");
    data = api->config_get(api->priv, "ui.lua.data", "");
    snprintf(rt->data_dir, sizeof rt->data_dir, "%s", data ? data : "");
    n = strlen(rt->data_dir);
    while (n > 1 && rt->data_dir[n - 1] == '/')
        rt->data_dir[--n] = 0;
    mem = api->config_get(api->priv, "ui.lua.memory_kb", "1024");
    steps = api->config_get(api->priv, "ui.lua.instructions", "100000");
    bytes = api->config_get(api->priv, "ui.lua.max_bytes", "32768");
    rt->memory_limit = (size_t)clamp_u(mem, 1024, 128, 4096) * 1024u;
    rt->max_steps = clamp_u(steps, 100000, 1000, 1000000);
    rt->max_bytes = clamp_u(bytes, 32768, 256, SCRIPT_MAX);
    g_rt = rt;
    return rt;
}

static void rt_destroy(struct runtime *rt)
{
    if (!rt)
        return;
    drop_pump(rt);
    if (rt->live) {
        drop_timers(rt, rt->live);
        close_session(rt, rt->live);
        rt->live = NULL;
    }
    free(rt->snap);
    if (g_rt == rt)
        g_rt = NULL;
    free(rt);
}

static int finish_start(struct runtime *rt)
{
    rt->quiescing = 0;
    rt->suspended = 0;
    if (publish_service(rt) != 0)
        return -1;
    if (arm_pump(rt) != 0) {
        unpublish_service(rt);
        return -1;
    }
    return 0;
}

static int rt_start(struct runtime *rt)
{
    struct session *s;
    if (!rt)
        return -1;
    rt->suspended = 0;
    rt->quiescing = 0;
    if (rt->from_blob) {
        rt->from_blob = 0;
        rt->error = 0;
        rt->message[0] = 0;
        if (!rt->live)
            return -1;
        return finish_start(rt);
    }
    if (!rt->path[0]) {
        rt->error = 0;
        rt->message[0] = 0;
        return finish_start(rt);
    }
    s = compile_path(rt, rt->path);
    if (!s)
        return -1;
    swap_session(rt, s);
    return finish_start(rt);
}

static void rt_stop(struct runtime *rt)
{
    if (!rt)
        return;
    drop_pump(rt);
    rt->suspended = 0;
    rt->in_call = 0;
    rt->from_blob = 0;
    rt->quiescing = 0;
    unpublish_service(rt);
    if (rt->live) {
        drop_timers(rt, rt->live);
        close_session(rt, rt->live);
        rt->live = NULL;
    }
    rt->generation++;
    rt->fetch_gen++;
}

static int rt_suspend(struct runtime *rt)
{
    if (!rt)
        return -1;
    drop_pump(rt);
    if (rt->live)
        cancel_session_fetches(rt, rt->live);
    drop_timers(rt, rt->live);
    rt->fetch_gen++;
    rt->suspended = 1;
    return 0;
}

static int rt_resume(struct runtime *rt)
{
    if (!rt)
        return -1;
    rt->quiescing = 0;
    if (arm_pump(rt) != 0)
        return -1;
    rt->suspended = 0;
    return 0;
}

static int encode_snap(struct runtime *rt, struct session *candidate)
{
    struct state_item items[STATES];
    int nitems = 0;
    struct cursor c = {0};
    size_t need;
    if (read_state_items(rt, rt->live ? rt->live->L : NULL, items, &nitems) != 0)
        return -1;
    c.cap = SIZE_MAX;
    put_bytes(&c, "OA1L", 4);
    put_u32(&c, 1);
    put_u32(&c, (uint32_t)candidate->source_len);
    write_model(&c, rt->live ? &rt->live->model : &candidate->model);
    write_items(&c, items, nitems);
    put_bytes(&c, candidate->source, candidate->source_len);
    if (c.overflow || c.n > 60 * 1024) {
        set_note(rt, EINVAL, "saved script state exceeds 60 KiB");
        return -1;
    }
    need = c.n;
    free(rt->snap);
    rt->snap = malloc(need);
    if (!rt->snap) {
        set_note(rt, ENOMEM, "out of memory");
        return -1;
    }
    memset(&c, 0, sizeof c);
    c.p = rt->snap;
    c.cap = need;
    put_bytes(&c, "OA1L", 4);
    put_u32(&c, 1);
    put_u32(&c, (uint32_t)candidate->source_len);
    write_model(&c, rt->live ? &rt->live->model : &candidate->model);
    write_items(&c, items, nitems);
    put_bytes(&c, candidate->source, candidate->source_len);
    if (c.overflow || c.n != need) {
        free(rt->snap);
        rt->snap = NULL;
        set_note(rt, EIO, "snap size changed");
        return -1;
    }
    rt->snap_len = need;
    return 0;
}

static size_t rt_save(struct runtime *rt, void *buf, size_t cap)
{
    struct session *candidate;
    if (!rt)
        return (size_t)-1;
    if (!buf || cap == 0) {
        free(rt->snap);
        rt->snap = NULL;
        rt->snap_len = 0;
        if (!rt->path[0] && !rt->live)
            return 0;
        if (!rt->path[0]) {
            set_note(rt, EINVAL, "script path");
            return (size_t)-1;
        }
        candidate = compile_path(rt, rt->path);
        if (!candidate)
            return (size_t)-1;
        if (encode_snap(rt, candidate) != 0) {
            close_session(rt, candidate);
            return (size_t)-1;
        }
        close_session(rt, candidate);
        return rt->snap_len;
    }
    if (!rt->snap || rt->snap_len > cap)
        return rt->snap ? rt->snap_len : (size_t)-1;
    memcpy(buf, rt->snap, rt->snap_len);
    return rt->snap_len;
}

static int rt_restore(struct runtime *rt, const void *buf, size_t len)
{
    struct reader r;
    struct session *s;
    struct model saved;
    struct state_item items[STATES];
    int nitems = 0;
    uint32_t version = 0, slen = 0;
    char magic[4];
    const char *src;
    if (!rt || !buf || len < 12)
        return -1;
    r.p = buf;
    r.n = len;
    r.i = 0;
    if (get_bytes(&r, magic, 4) != 0 || memcmp(magic, "OA1L", 4) != 0 ||
        get_u32(&r, &version) != 0 || version != 1 || get_u32(&r, &slen) != 0 ||
        slen == 0 || slen > rt->max_bytes)
        return -1;
    if (read_model(&r, &saved) != 0 || read_items(&r, items, &nitems) != 0)
        return -1;
    if (slen > r.n - r.i || r.n - r.i != slen)
        return -1;
    src = (const char *)(r.p + r.i);
    s = new_session(rt);
    if (!s) {
        set_note(rt, ENOMEM, "not enough memory");
        return -1;
    }
    s->source = malloc(slen);
    if (!s->source) {
        close_session(rt, s);
        set_note(rt, ENOMEM, "out of memory");
        return -1;
    }
    memcpy(s->source, src, slen);
    s->source_len = slen;
    if (run_source(rt, s, s->source, s->source_len) != 0 ||
        adopt(rt, s, &saved, items, nitems) != 0) {
        close_session(rt, s);
        return -1;
    }
    swap_session(rt, s);
    rt->from_blob = 1;
    return 0;
}

static int count_timers(const struct session *s)
{
    int i, n = 0;
    if (!s)
        return 0;
    for (i = 0; i < TIMERS; i++)
        if (s->slots[i].id)
            n++;
    return n;
}

static int svc_status(oa_lua_status *out)
{
    struct runtime *rt = g_rt;
    if (!out)
        return -1;
    memset(out, 0, sizeof *out);
    if (!rt)
        return -1;
    out->loaded = rt->live ? 1 : 0;
    out->error = rt->error;
    out->timers = count_timers(rt->live);
    out->has_theme = rt->live && rt->live->model.has_theme;
    out->has_nav = rt->live && rt->live->model.has_nav;
    out->generation = rt->generation;
    out->api = rt->live ? OA_LUA_API_VERSION : 0;
    snprintf(out->message, sizeof out->message, "%s", rt->message);
    return 0;
}

static int svc_reload(const char *path)
{
    struct runtime *rt = g_rt;
    struct session *s;
    struct state_item items[STATES];
    int nitems = 0;
    struct model saved;
    int have_model = 0;
    const char *use;
    /* Quiesce has unpublished the service and may still be reaping a child.
     * A script swap here would block in waitpid and free that session. */
    if (!rt || rt->suspended || rt->quiescing || rt->in_call)
        return -1;
    use = path && path[0] ? path : rt->path;
    if (rt->live) {
        saved = rt->live->model;
        have_model = 1;
        if (read_state_items(rt, rt->live->L, items, &nitems) != 0)
            return -1;
    }
    s = compile_path(rt, use);
    if (!s)
        return -1;
    if (adopt(rt, s, have_model ? &saved : NULL, items, nitems) != 0) {
        close_session(rt, s);
        return -1;
    }
    if (path && path[0])
        snprintf(rt->path, sizeof rt->path, "%s", path);
    swap_session(rt, s);
    return 0;
}

static int svc_layout_count(void)
{
    return g_rt && g_rt->live ? g_rt->live->model.nlayout : 0;
}

static int svc_layout_at(int index, oa_lua_layout *out)
{
    struct model *m;
    if (!out || !g_rt || !g_rt->live)
        return -1;
    m = &g_rt->live->model;
    if (index < 0 || index >= m->nlayout)
        return -1;
    memset(out, 0, sizeof *out);
    memcpy(out->name, m->layouts[index].name, OA_LUA_NAME_MAX);
    out->name[OA_LUA_NAME_MAX - 1] = 0;
    out->x = m->layouts[index].x;
    out->y = m->layouts[index].y;
    out->w = m->layouts[index].w;
    out->h = m->layouts[index].h;
    return 0;
}

static int svc_nav(int *band, int *index)
{
    if (!band || !index || !g_rt || !g_rt->live || !g_rt->live->model.has_nav)
        return -1;
    *band = g_rt->live->model.nav_band;
    *index = g_rt->live->model.nav_index;
    return 0;
}

static int svc_theme(int bg[4], int accent[4])
{
    int i;
    if (!bg || !accent || !g_rt || !g_rt->live || !g_rt->live->model.has_theme)
        return -1;
    for (i = 0; i < 4; i++) {
        bg[i] = g_rt->live->model.bg[i];
        accent[i] = g_rt->live->model.accent[i];
    }
    return 0;
}

static int svc_binding(int key, char *dst, size_t cap)
{
    struct model *m;
    int i;
    if (!dst || cap == 0 || key < 0 || key > 255 || !g_rt || !g_rt->live)
        return -1;
    m = &g_rt->live->model;
    for (i = 0; i < m->nbind; i++)
        if (m->binds[i].used && m->binds[i].key == key) {
            snprintf(dst, cap, "%s", m->binds[i].action);
            return 0;
        }
    dst[0] = 0;
    return 1;
}

static int svc_anim(const char *id, int *duration_ms, int *easing)
{
    struct model *m;
    int i;
    if (!id || !id[0] || !duration_ms || !easing || !g_rt || !g_rt->live)
        return -1;
    m = &g_rt->live->model;
    for (i = 0; i < m->nanim; i++)
        if (m->anims[i].used && !strcmp(m->anims[i].id, id)) {
            *duration_ms = m->anims[i].duration;
            *easing = m->anims[i].easing;
            return 0;
        }
    return 1;
}

static int hook_name(const char *hook, const char **global)
{
    if (!hook)
        return -1;
    if (!strcmp(hook, "paint"))
        *global = "oa_paint";
    else if (!strcmp(hook, "key"))
        *global = "oa_key";
    else if (!strcmp(hook, "enter"))
        *global = "oa_enter";
    else if (!strcmp(hook, "leave"))
        *global = "oa_leave";
    else
        return -1;
    return 0;
}

static int skin_name_ok(const char *skin)
{
    size_t i, n;
    if (!skin || !skin[0])
        return 0;
    n = strlen(skin);
    if (n >= OA_LUA_NAME_MAX)
        return 0;
    for (i = 0; i < n; i++) {
        char c = skin[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return 1;
}

static int svc_call(const char *hook, const char *skin, int arg, int *handled)
{
    struct runtime *rt = g_rt;
    struct session *s;
    struct model saved;
    const char *global = NULL;
    if (handled)
        *handled = 0;
    if (!rt || !skin_name_ok(skin) || hook_name(hook, &global) != 0)
        return -1;
    /* Not handled: the vendor skin must still see the key while cleanup
     * is pending. The VM stays alive until stop, but hooks must not run. */
    if (!rt->live || rt->suspended || rt->quiescing)
        return 0;
    if (rt->in_call) {
        set_note(rt, EBUSY, "lua hook re-entered");
        return -1;
    }
    s = rt->live;
    saved = s->model;
    lua_getglobal(s->L, global);
    if (!lua_isfunction(s->L, -1)) {
        lua_pop(s->L, 1);
        return 0;
    }
    lua_pushstring(s->L, skin);
    lua_pushinteger(s->L, arg);
    rt->in_call = 1;
    if (protected(rt, s, 2, 1) != 0) {
        s->model = saved;
        rt->in_call = 0;
        return -1;
    }
    rt->in_call = 0;
    rt->error = 0;
    rt->message[0] = 0;
    if (handled) {
        if (lua_isnil(s->L, -1))
            *handled = 0;
        else if (lua_isinteger(s->L, -1))
            *handled = (int)lua_tointeger(s->L, -1);
        else if (lua_isboolean(s->L, -1))
            *handled = lua_toboolean(s->L, -1) ? 1 : 0;
        else {
            lua_pop(s->L, 1);
            s->model = saved;
            set_note(rt, EINVAL, "hook must return nil, bool, or integer");
            return -1;
        }
    }
    lua_pop(s->L, 1);
    return 0;
}

static const oa_lua_iface g_iface = {
    svc_status, svc_reload, svc_layout_count, svc_layout_at, svc_nav, svc_theme,
    svc_binding, svc_anim, svc_call
};

/* Module entry points. One runtime per process; the service table above
 * uses it. Skins only see oa_lua_iface. */
struct oa_lua_runtime {
    struct runtime *rt;
};

struct oa_lua_runtime *oa_lua_create(const oa_host_api *api)
{
    struct oa_lua_runtime *wrap = calloc(1, sizeof *wrap);
    if (!wrap)
        return NULL;
    wrap->rt = rt_create(api);
    if (!wrap->rt) {
        free(wrap);
        return NULL;
    }
    return wrap;
}

void oa_lua_destroy(struct oa_lua_runtime *wrap)
{
    if (!wrap)
        return;
    rt_destroy(wrap->rt);
    free(wrap);
}

int oa_lua_start(struct oa_lua_runtime *wrap) { return wrap ? rt_start(wrap->rt) : -1; }
void oa_lua_stop(struct oa_lua_runtime *wrap) { if (wrap) rt_stop(wrap->rt); }
int oa_lua_suspend(struct oa_lua_runtime *wrap) { return wrap ? rt_suspend(wrap->rt) : -1; }
int oa_lua_resume(struct oa_lua_runtime *wrap) { return wrap ? rt_resume(wrap->rt) : -1; }
size_t oa_lua_save(struct oa_lua_runtime *wrap, void *buf, size_t cap)
{
    return wrap ? rt_save(wrap->rt, buf, cap) : (size_t)-1;
}
int oa_lua_restore(struct oa_lua_runtime *wrap, const void *buf, size_t len)
{
    return wrap ? rt_restore(wrap->rt, buf, len) : -1;
}
int oa_lua_quiesce(struct oa_lua_runtime *wrap)
{
    return rt_quiesce(wrap ? wrap->rt : NULL);
}
const oa_lua_iface *oa_lua_service(void) { return &g_iface; }
