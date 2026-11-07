#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "host.h"
#include "openaurora/app_launch.h"
#include "openaurora/events.h"

static char helper_path[512];
static char map_path[512];
static char out_path[512];
static const char *enable = "1";

static const char *config(void *ud, const char *key, const char *def)
{
    (void)ud;
    if (!strcmp(key, "app_launch.enable")) return enable;
    if (!strcmp(key, "app_launch.helper")) return helper_path;
    if (!strcmp(key, "app_launch.map")) return map_path;
    if (!strcmp(key, "app_launch.reap_ms")) return "20";
    return def;
}

static void pump(oa_host *h)
{
    for (int i = 0; i < MAX_TIMERS; i++) if (h->timers[i].active) {
        oa_timer t = h->timers[i]; h->cur_module = t.owner;
        t.cb(t.ud); h->cur_module = 0;
    }
    for (int i = 0; i < MAX_WATCHES; i++) if (h->watches[i].active) {
        oa_watch w = h->watches[i];
        struct pollfd p = { .fd = w.fd, .events = POLLIN };
        if (poll(&p, 1, 0) == 1) {
            h->cur_module = w.owner;
            w.cb(w.ud, w.fd, OA_WATCH_READ);
            h->cur_module = 0;
        }
    }
    const struct timespec pause = { .tv_nsec = 1000000 };
    nanosleep(&pause, NULL);
}

static void done(oa_host *h)
{
    h->stop_requested = 1;
    assert(oa_host_run(h) == 0);
    int status;
    assert(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
    close(h->selfpipe[0]);
    close(h->selfpipe[1]);
    pthread_mutex_destroy(&h->post_mu);
}

static void write_file(const char *path, const char *text, mode_t mode)
{
    FILE *f = fopen(path, "w");
    assert(f);
    assert(fputs(text, f) >= 0);
    assert(fclose(f) == 0);
    assert(chmod(path, mode) == 0);
}

static int read_file(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    size_t n = fread(out, 1, cap - 1, f);
    int failed = ferror(f);
    fclose(f);
    if (failed) return -1;
    out[n] = 0;
    return (int)n;
}

static void emit_select(oa_host *h, const char *name)
{
    oa_action_event act;
    memset(&act, 0, sizeof act);
    act.kind = OA_ACTION_SELECT;
    act.name = name;
    h->api.emit(h, OA_TOPIC_ACTION, &act);
}

static void wait_output(oa_host *h, char *out, size_t cap)
{
    for (int i = 0; i < 2000; i++) {
        if (read_file(out_path, out, cap) > 0) return;
        pump(h);
    }
    assert(0 && "helper output not observed");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *modules = argv[1];
    char dir[] = "/tmp/oa-app-launch-test-XXXXXX";
    assert(mkdtemp(dir));
    snprintf(helper_path, sizeof helper_path, "%s/helper.sh", dir);
    snprintf(map_path, sizeof map_path, "%s/apps.map", dir);
    snprintf(out_path, sizeof out_path, "%s/out.txt", dir);
    write_file(helper_path,
               "#!/bin/sh\n"
               "printf '%s\\n' \"$@\" > \"$OA_APP_LAUNCH_OUT\"\n",
               0755);
    write_file(map_path,
               "# display name<TAB>program<TAB>args\n"
               "Netflix\tnetflix\thttps://example.test/watch\n"
               "YouTube\tyoutube\t\n",
               0644);
    assert(setenv("OA_APP_LAUNCH_OUT", out_path, 1) == 0);

    oa_host h;
    assert(oa_host_init(&h, modules, NULL) == 0);
    h.log_level = OA_LOG_ERROR;
    h.api.config_get = config;
    oa_mod *m = oa_module_load(&h, "app_launch");
    assert(m && oa_module_start(&h, m) == 0);
    const oa_app_launch_iface *svc =
        h.api.lookup(&h, OA_SVC_APP_LAUNCH, 1);
    assert(svc);

    unlink(out_path);
    emit_select(&h, "Netflix");
    char out[512];
    wait_output(&h, out, sizeof out);
    assert(strstr(out, "--name\nnetflix\n"));
    assert(strstr(out, "--args\nhttps://example.test/watch\n"));
    for (int i = 0; i < 1000; i++) {
        int status;
        if (waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD) break;
        pump(&h);
    }

    unlink(out_path);
    assert(svc->request("YouTube", "custom-arg") == 0);
    wait_output(&h, out, sizeof out);
    assert(strstr(out, "--name\nyoutube\n"));
    assert(strstr(out, "--args\ncustom-arg\n"));
    for (int i = 0; i < 1000; i++) {
        int status;
        if (waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD) break;
        pump(&h);
    }

    unlink(out_path);
    errno = 0;
    assert(svc->request("Unknown", NULL) == -1 && errno == ENOENT);
    pump(&h);
    assert(read_file(out_path, out, sizeof out) < 0);

    assert(oa_module_unload(&h, "app_launch", 0) == 0);
    enable = "0";
    m = oa_module_load(&h, "app_launch");
    assert(m && oa_module_start(&h, m) == 0);
    unlink(out_path);
    emit_select(&h, "Netflix");
    for (int i = 0; i < 50; i++) pump(&h);
    assert(read_file(out_path, out, sizeof out) < 0);
    assert(oa_module_unload(&h, "app_launch", 0) == 0);
    done(&h);
    puts("ok app_launch: mapped spawn, explicit args, unknown/disabled rejection");
    return 0;
}
