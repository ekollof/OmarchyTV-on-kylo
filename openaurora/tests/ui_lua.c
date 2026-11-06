/* Shared ui.lua service: every skin can call it; it is not itself a skin.
 * Drives the shipped modules and host control parser. */
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "host.h"
#include "openaurora/events.h"
#include "openaurora/lua_skin.h"
#include "ui_nav_trace.h"

static char g_root[] = "/tmp/oa-ui-lua-XXXXXX";
static char g_script[256];
static char g_data[256];
static char g_skin[32] = "omarchy";
static int g_require_lua = 1;
static const char *config(void *ud, const char *key, const char *def)
{
    oa_host *h = ud;
    if (!strcmp(key, "ui.skin")) {
        if (h && h->skin_selected[0])
            return h->skin_selected;
        return g_skin;
    }
    if (!strcmp(key, "ui.ok_file"))
        return "";
    if (!strcmp(key, "ui.fps"))
        return "0";
    if (!strcmp(key, "ui.dir"))
        return g_root;
    if (!strcmp(key, "ui.lua.script"))
        return g_script;
    if (!strcmp(key, "ui.lua.data"))
        return g_data;
    if (!strcmp(key, "ui.lua.control"))
        return "2";
    if (!strcmp(key, "ui.omarchy.lua"))
        return g_require_lua ? "1" : "0";
    if (!strcmp(key, "ui.paint"))
        return "1";
    if (!strcmp(key, "ui.focus"))
        return "2";
    if (!strcmp(key, "ui.skin_timeout_ms"))
        return "400";
    return def;
}

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[160];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

static int has_fill(const oa_nav_trace *trace, int x, int y, int w, int h, int r, int g, int b)
{
    int n = trace->fill_count();
    for (int i = 0; i < n; i++) {
        int fx, fy, fw, fh, fr, fg, fb;
        if (trace->fill_at(i, &fx, &fy, &fw, &fh, &fr, &fg, &fb) == 0 &&
            fx == x && fy == y && fw == w && fh == h && fr == r && fg == g && fb == b)
            return 1;
    }
    return 0;
}

static void fire_due(oa_host *h)
{
    uint64_t now = oa_now_ms();
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!h->timers[i].active || h->timers[i].due > now)
            continue;
        oa_timer t = h->timers[i];
        if (t.repeat_ms)
            h->timers[i].due = now + t.repeat_ms;
        else
            h->timers[i].active = 0;
        uint32_t prev = h->cur_module;
        h->cur_module = t.owner;
        t.cb(t.ud);
        h->cur_module = prev;
    }
}

static void drive(oa_host *h, int rounds)
{
    for (int n = 0; n < rounds; n++) {
        fire_due(h);
        struct pollfd pfds[MAX_WATCHES];
        oa_watch slots[MAX_WATCHES];
        int np = 0;
        for (int i = 0; i < MAX_WATCHES && np < MAX_WATCHES; i++) {
            if (!h->watches[i].active)
                continue;
            slots[np] = h->watches[i];
            pfds[np].fd = h->watches[i].fd;
            pfds[np].events = POLLIN;
            pfds[np].revents = 0;
            np++;
        }
        if (np == 0)
            usleep(20000);
        else
            poll(pfds, (nfds_t)np, 20);
        for (int i = 0; i < np; i++) {
            if (!pfds[i].revents)
                continue;
            uint32_t prev = h->cur_module;
            h->cur_module = slots[i].owner;
            slots[i].cb(slots[i].ud, slots[i].fd, OA_WATCH_READ);
            h->cur_module = prev;
        }
        fire_due(h);
        oa_host_dispatch_posts(h);
    }
}

static void write_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    assert(f);
    assert(fputs(text, f) >= 0);
    assert(!fclose(f));
}

static const oa_lua_iface *hold(oa_host *h)
{
    const oa_lua_iface *lua = h->api.lookup(h, OA_SVC_UI_LUA, 1);
    assert(lua);
    return lua;
}

static void drop(oa_host *h)
{
    h->api.release(h, OA_SVC_UI_LUA);
}

static int start_server(int port, const char *dir)
{
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        int nul = open("/dev/null", O_RDWR);
        if (nul >= 0) {
            dup2(nul, 1);
            dup2(nul, 2);
            if (nul > 2)
                close(nul);
        }
        if (chdir(dir) != 0)
            _exit(127);
        char p[16];
        snprintf(p, sizeof p, "%d", port);
        execl("/usr/bin/python3", "python3", "-m", "http.server", p, "--bind", "127.0.0.1", (char *)NULL);
        _exit(127);
    }
    return (int)pid;
}

static pid_t find_child(const char *comm)
{
    DIR *d = opendir("/proc");
    struct dirent *e;
    pid_t self = getpid();
    pid_t found = 0;
    if (!d)
        return 0;
    while ((e = readdir(d))) {
        char path[64], buf[256], name[64];
        char *left, *right;
        ssize_t n;
        int fd;
        pid_t pid = (pid_t)atoi(e->d_name);
        int ppid = 0;
        if (pid <= 0)
            continue;
        snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
        fd = open(path, O_RDONLY);
        if (fd < 0)
            continue;
        n = read(fd, buf, sizeof buf - 1);
        close(fd);
        if (n <= 0)
            continue;
        buf[n] = 0;
        left = strchr(buf, '(');
        right = strrchr(buf, ')');
        if (!left || !right || right < left || (size_t)(right - left - 1) >= sizeof name)
            continue;
        memcpy(name, left + 1, (size_t)(right - left - 1));
        name[right - left - 1] = 0;
        if (sscanf(right + 2, "%*c %d", &ppid) != 1 || ppid != self)
            continue;
        if (!strcmp(name, comm)) {
            found = pid;
            break;
        }
    }
    closedir(d);
    return found;
}

/* Accept connections and never write a response, so curl stays running. */
static int hang_server(int port)
{
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        int s = socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        struct sockaddr_in in;
        if (s < 0)
            _exit(127);
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        memset(&in, 0, sizeof in);
        in.sin_family = AF_INET;
        in.sin_port = htons((uint16_t)port);
        in.sin_addr.s_addr = htonl(0x7f000001);
        if (bind(s, (struct sockaddr *)&in, sizeof in) != 0 || listen(s, 4) != 0)
            _exit(127);
        for (;;) {
            int c = accept(s, NULL, NULL);
            if (c < 0 && errno != EINTR)
                _exit(0);
        }
    }
    return (int)pid;
}

static int server_up(int port)
{
    for (int i = 0; i < 50; i++) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in in;
        if (fd < 0)
            return 0;
        memset(&in, 0, sizeof in);
        in.sin_family = AF_INET;
        in.sin_port = htons((uint16_t)port);
        in.sin_addr.s_addr = htonl(0x7f000001);
        if (connect(fd, (struct sockaddr *)&in, sizeof in) == 0) {
            close(fd);
            return 1;
        }
        close(fd);
        usleep(20000);
    }
    return 0;
}

int main(int argc, char **argv)
{
    char out[256], stat_path[320], out_path[320], good[320], bad[320], bomb[320], spin[320], file_url[320], fetch[320];
    oa_host host;
    const oa_nav_trace *trace;
    const oa_lua_iface *lua;
    oa_lua_status st;
    int band = 0, index = 0;
    int port = 20000 + (getpid() % 20000);
    int server = -1;
    char url[80];
    assert(argc == 2);
    assert(mkdtemp(g_root));
    snprintf(g_data, sizeof g_data, "%s/data", g_root);
    assert(!mkdir(g_data, 0700));
    snprintf(g_script, sizeof g_script, "%s/good.lua", g_root);
    snprintf(stat_path, sizeof stat_path, "%s/stat.txt", g_root);
    snprintf(out_path, sizeof out_path, "%s/out.txt", g_data);
    write_file(stat_path, "stat-ok");
    snprintf(good, sizeof good, "%s/good.lua", g_root);
    {
        FILE *f = fopen(good, "w");
        assert(f);
        fprintf(f,
            "assert(debug == nil and package == nil and coroutine == nil)\n"
            "assert(require == nil and load == nil and loadfile == nil and dofile == nil)\n"
            "assert(os.execute == nil and os.exit == nil and os.remove == nil and os.rename == nil)\n"
            "assert(io.popen == nil and io.stdin == nil and io.stdout == nil and io.stderr == nil)\n"
            "assert(type(os.time()) == 'number')\n"
            "assert(type(os.date()) == 'string')\n"
            "local f = io.open('%s', 'r')\n"
            "assert(f)\n"
            "oa.state.stat = f:read('*a')\n"
            "f:close()\n"
            "assert(oa.state.stat == 'stat-ok')\n"
            "assert(io.open('/dev/null') == nil)\n"
            "local w = io.open('%s', 'w')\n"
            "assert(w)\n"
            "w:write('z')\n"
            "w:close()\n"
            "assert(io.open('/tmp/oa-lua-nope', 'w') == nil)\n"
            "oa.theme(9, 8, 7, 255, 6, 5, 4, 255)\n"
            "oa.layout('rail', 0, 0, 40, 100)\n"
            "function oa_key(skin, key)\n"
            "  if skin == 'omarchy' and key == 19 then\n"
            "    oa.theme(9, 8, 7, 255, 9, 9, 9, 255)\n"
            "    return 1\n"
            "  end\n"
            "end\n"
            "function oa_paint(skin)\n"
            "  if skin == 'omarchy' and not oa.state.armed then\n"
            "    oa.state.armed = true\n"
            "    oa.after(60000, function() end)\n"
            "  elseif skin == 'vendor' then\n"
            "    oa.theme(1, 2, 3, 255, 4, 5, 6, 255)\n"
            "  end\n"
            "end\n"
            "function oa_migrate(prev)\n"
            "  local n = 0\n"
            "  if type(prev) == 'table' and type(prev.n) == 'number' then n = prev.n end\n"
            "  n = n + 1\n"
            "  oa.nav(1, n)\n"
            "  prev.n = n\n"
            "  return prev\n"
            "end\n",
            stat_path, out_path);
        assert(!fclose(f));
    }
    assert(!oa_host_init(&host, argv[1], NULL));
    host.log_level = OA_LOG_ERROR;
    host.api.config_get = config;
    assert(!command(&host, "load nav_display", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start nav_display", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load ui_engine", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_engine", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load lua", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start lua", out, sizeof out) && !strcmp(out, "ok\n"));
    lua = hold(&host);
    assert(!lua->status(&st) && st.loaded && st.api == 1 && !st.error);
    {
        FILE *f = fopen(out_path, "r");
        char buf[8] = {0};
        assert(f && fread(buf, 1, 1, f) == 1 && buf[0] == 'z' && !fclose(f));
    }
    drop(&host);
    puts("  ok   sandbox keeps io/os for regular files and the clock, and rejects shell and device opens");

    assert(!command(&host, "load ui_omarchy", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
    trace = host.api.lookup(&host, OA_SVC_NAV_TRACE, 1);
    assert(trace);
    assert(has_fill(trace, 0, 0, 40, 100, 6, 5, 4));
    assert(!has_fill(trace, 0, 0, 14, 1080, 0, 122, 204));
    lua = hold(&host);
    assert(!lua->status(&st) && st.timers == 1);
    drop(&host);
    {
        oa_key_event e = {.id = 19, .kind = OA_KEY_PRESS};
        host.api.emit(&host, OA_TOPIC_KEY, &e);
        oa_host_dispatch_posts(&host);
    }
    assert(has_fill(trace, 0, 0, 40, 100, 9, 9, 9));
    puts("  ok   omarchy paints the script theme and layout, and a key updates them");

    assert(!command(&host, "suspend", out, sizeof out) && !strcmp(out, "ok\n"));
    lua = hold(&host);
    assert(!lua->status(&st) && st.loaded && st.timers == 0);
    drop(&host);
    {
        /* Suspend already dropped the pump. A full timer table must keep the
         * module suspended; restoring the previous slots lets resume arm it. */
        oa_mod *lm = oa_host_find_module(&host, "lua");
        int saved[MAX_TIMERS];
        uint32_t prev = host.cur_module;
        assert(lm && lm->state == OA_ST_SUSPENDED && lm->desc->resume);
        for (int i = 0; i < MAX_TIMERS; i++) {
            saved[i] = host.timers[i].active;
            host.timers[i].active = 1;
        }
        host.cur_module = lm->id;
        assert(lm->desc->resume(lm->ctx) == -1);
        host.cur_module = prev;
        assert(lm->state == OA_ST_SUSPENDED);
        for (int i = 0; i < MAX_TIMERS; i++)
            host.timers[i].active = saved[i];
        lua = hold(&host);
        assert(!lua->status(&st) && st.loaded && st.timers == 0);
        drop(&host);
        puts("  ok   resume stays suspended when the fetch pump cannot be allocated");
    }
    assert(!command(&host, "resume", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(has_fill(trace, 0, 0, 40, 100, 9, 9, 9));
    puts("  ok   suspend drops script timers and resume still paints the script");

    assert(command(&host, "reload lua", out, sizeof out) != 0 && !strcmp(out, "failed\n"));
    assert(oa_host_find_module(&host, "lua")->state == OA_ST_STARTED);
    assert(oa_host_find_module(&host, "ui_omarchy")->state == OA_ST_STARTED);
    assert(!command(&host, "stop ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "reload lua", out, sizeof out) && !strcmp(out, "ok\n"));
    lua = hold(&host);
    assert(!lua->nav(&band, &index) && band == 1 && index == 1);
    snprintf(bad, sizeof bad, "%s/bad.lua", g_root);
    snprintf(bomb, sizeof bomb, "%s/bomb.lua", g_root);
    snprintf(spin, sizeof spin, "%s/spin.lua", g_root);
    snprintf(file_url, sizeof file_url, "%s/file.lua", g_root);
    write_file(bad, "function(\n");
    write_file(bomb, "string.rep('x', 8*1024*1024)\n");
    write_file(spin, "local i=0\nwhile true do i=i+1 end\n");
    write_file(file_url, "oa.fetch('file:///etc/passwd', function() end)\n");
    assert(lua->reload_script(bad) == -1);
    assert(lua->reload_script(bomb) == -1);
    assert(lua->reload_script(spin) == -1);
    assert(lua->reload_script(file_url) == -1);
    assert(!lua->nav(&band, &index) && band == 1 && index == 1);
    assert(!lua->status(&st) && st.loaded);
    drop(&host);
    puts("  ok   a held service refuses module reload; a bad candidate keeps the running script");

    {
        char boom[320];
        int bg[4], ac[4];
        snprintf(boom, sizeof boom, "%s/boom.lua", g_root);
        write_file(boom,
            "function oa_paint()\n"
            "  oa.theme(3, 3, 3, 255, 3, 3, 3, 255)\n"
            "  error('boom')\n"
            "end\n");
        lua = hold(&host);
        assert(!lua->reload_script(boom));
        assert(lua->call("paint", "omarchy", 0, NULL) == -1);
        assert(!lua->theme(bg, ac) && bg[0] == 9 && bg[1] == 8 && bg[2] == 7);
        assert(!lua->status(&st) && st.error != 0);
        assert(!lua->reload_script(g_script));
        drop(&host);
        unlink(boom);
    }
    puts("  ok   a paint hook that errors restores the previous theme");

    strcpy(g_skin, "vendor");
    assert(!command(&host, "load ui_vendor", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start ui_vendor", out, sizeof out) && !strcmp(out, "ok\n"));
    lua = hold(&host);
    {
        int bg[4], ac[4];
        assert(!lua->theme(bg, ac) && bg[0] == 1 && bg[1] == 2 && bg[2] == 3);
    }
    drop(&host);
    assert(!has_fill(trace, 0, 0, 40, 100, 1, 2, 3));
    assert(!has_fill(trace, 11, 451, 150, 150, 1, 2, 3));
    puts("  ok   vendor runs the paint hook and does not draw the script colours");
    assert(!command(&host, "stop ui_vendor", out, sizeof out));
    assert(!command(&host, "unload ui_vendor", out, sizeof out));

    snprintf(url, sizeof url, "http://127.0.0.1:%d/wx.txt", port);
    snprintf(fetch, sizeof fetch, "%s/fetch.lua", g_root);
    {
        FILE *f = fopen(fetch, "w");
        assert(f);
        fprintf(f,
            "oa.theme(9, 8, 7, 255, 6, 5, 4, 255)\n"
            "oa.layout('rail', 0, 0, 40, 100)\n"
            "function oa_paint(skin)\n"
            "  if skin ~= 'omarchy' or oa.state.did then return end\n"
            "  oa.state.did = true\n"
            "  oa.fetch('%s', function(body, err)\n"
            "    if not body then oa.nav(3, 1)\n"
            "    elseif #body == 9 then oa.nav(3, 9)\n"
            "    else oa.nav(3, 2) end\n"
            "  end)\n"
            "end\n",
            url);
        assert(!fclose(f));
    }
    {
        char body[320];
        snprintf(body, sizeof body, "%s/wx.txt", g_root);
        write_file(body, "hello-rss");
    }
    lua = hold(&host);
    assert(!lua->reload_script(fetch));
    drop(&host);
    strcpy(g_skin, "omarchy");
    server = start_server(port, g_root);
    if (server_up(port)) {
        assert(!command(&host, "start ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
        drive(&host, 80);
        lua = hold(&host);
        assert(!lua->nav(&band, &index) && band == 3 && index == 9);
        drop(&host);
        puts("  ok   oa.fetch reads a local http body without blocking the host loop");
    } else {
        assert(!command(&host, "start ui_omarchy", out, sizeof out) && !strcmp(out, "ok\n"));
        drive(&host, 40);
        lua = hold(&host);
        assert(!lua->nav(&band, &index) && band == 3 && index == 1);
        drop(&host);
        puts("  ok   oa.fetch reports a failed http transfer");
    }
    if (server > 0) {
        kill(server, SIGTERM);
        waitpid(server, NULL, 0);
    }
    {
        char aurora_path[320];
        snprintf(aurora_path, sizeof aurora_path, "%s/aurora.lua", g_root);
        write_file(aurora_path,
            "function oa_paint(skin)\n"
            "  local reply, ok = aurora.query('list')\n"
            "  if ok and reply:find('lua', 1, true) then oa.nav(8, 1) end\n"
            "  if oa.state.req == nil then\n"
            "    oa.state.req = aurora.request('status lua')\n"
            "  end\n"
            "  local result = aurora.result()\n"
            "  if result and result.ok and result.reply:find('lua', 1, true) then\n"
            "    oa.nav(9, 2)\n"
            "  end\n"
            "  if not pcall(function() aurora.request('stop lua') end) then\n"
            "    oa.nav(10, 3)\n"
            "  end\n"
            "  if not pcall(function() aurora.request('load /tmp/evil.so') end) then\n"
            "    oa.nav(11, 4)\n"
            "  end\n"
            "end\n");
        lua = hold(&host);
        assert(lua->reload_script(aurora_path) == 0);
        drop(&host);
        lua = hold(&host);
        int handled = 0;
        assert(lua->call("paint", "omarchy", 0, &handled) == 0);
        drop(&host);
        oa_host_dispatch_posts(&host);
        lua = hold(&host);
        assert(lua->call("paint", "omarchy", 0, &handled) == 0);
        assert(lua->nav(&band, &index) == 0 && band == 11 && index == 4);
        drop(&host);
        puts("  ok   aurora query/request bridges a read-only and deferred control command");
        unlink(aurora_path);
    }
    assert(!command(&host, "stop ui_omarchy", out, sizeof out));
    assert(!command(&host, "unload ui_omarchy", out, sizeof out));
    {
        int hang_port = port + 1;
        int hang = hang_server(hang_port);
        int handled = 0;
        int q;
        char hang_path[320];
        oa_mod *lua_mod;
        uint64_t end;
        assert(server_up(hang_port));
        snprintf(hang_path, sizeof hang_path, "%s/hang.lua", g_root);
        {
            FILE *f = fopen(hang_path, "w");
            assert(f);
            fprintf(f,
                "oa.nav(1, 1)\n"
                "function oa_paint(skin)\n"
                "  oa.nav(4, 7)\n"
                "  oa.fetch('http://127.0.0.1:%d/hang', function(body, err)\n"
                "    oa.nav(5, 5)\n"
                "  end)\n"
                "  return 1\n"
                "end\n",
                hang_port);
            assert(!fclose(f));
        }
        lua = hold(&host);
        assert(lua->reload_script(hang_path) == 0);
        assert(lua->call("paint", "omarchy", 0, &handled) == 0 && handled == 1);
        assert(lua->nav(&band, &index) == 0 && band == 4 && index == 7);
        drop(&host);
        drive(&host, 15);
        lua_mod = oa_host_find_module(&host, "lua");
        assert(lua_mod && lua_mod->desc->quiesce);
        {
            /* Freeze curl so the SIGTERM inside quiesce cannot reap it
             * before waitpid(WNOHANG). A stopped child is not a zombie. */
            pid_t curl = find_child("curl");
            assert(curl > 0);
            assert(kill(curl, SIGSTOP) == 0);
        }
        q = lua_mod->desc->quiesce(lua_mod->ctx);
        assert(q == -1);
        handled = 0;
        assert(lua->call("paint", "omarchy", 0, &handled) == 0 && handled == 0);
        assert(lua->nav(&band, &index) == 0 && band == 4 && index == 7);
        assert(lua->reload_script(g_script) == -1);
        assert(lua->nav(&band, &index) == 0 && band == 4 && index == 7);
        kill(hang, SIGTERM);
        waitpid(hang, NULL, 0);
        end = oa_now_ms() + 3000;
        do {
            q = lua_mod->desc->quiesce(lua_mod->ctx);
            if (q == 0)
                break;
            usleep(20000);
        } while (oa_now_ms() < end);
        assert(q == 0);
        unlink(hang_path);
        puts("  ok   quiesce unpublishes lua and rejects hook/reload while curl is unreaped");
    }
    assert(!command(&host, "stop lua", out, sizeof out));
    assert(!command(&host, "unload lua", out, sizeof out));
    g_script[0] = 0;
    assert(!oa_skin_request(&host, "omarchy"));
    {
        uint64_t end = oa_now_ms() + 3000;
        while (host.skin_timer) {
            assert(oa_now_ms() < end);
            oa_host_dispatch_posts(&host);
            fire_due(&host);
            usleep(1000);
        }
    }
    assert(!command(&host, "skin", out, sizeof out));
    assert(strstr(out, "active vendor"));
    assert(strstr(out, "fallback 1"));
    assert(oa_host_find_module(&host, "ui_vendor"));
    assert(!oa_host_find_module(&host, "ui_omarchy") ||
           oa_host_find_module(&host, "ui_omarchy")->state != OA_ST_STARTED);
    puts("  ok   ui.omarchy.lua without a loaded script falls back to the vendor skin");

    host.stop_requested = 1;
    assert(!oa_host_run(&host));
    close(host.selfpipe[0]);
    close(host.selfpipe[1]);
    pthread_mutex_destroy(&host.post_mu);
    unlink(stat_path);
    unlink(out_path);
    unlink(good);
    unlink(bad);
    unlink(bomb);
    unlink(spin);
    unlink(file_url);
    unlink(fetch);
    {
        char body[320];
        snprintf(body, sizeof body, "%s/wx.txt", g_root);
        unlink(body);
    }
    rmdir(g_data);
    rmdir(g_root);
    return 0;
}
