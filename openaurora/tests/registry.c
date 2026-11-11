/* Regression coverage for the checked-off loader claims audited on 2026-10-31. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "host.h"

static void close_host(oa_host *h)
{
    close(h->selfpipe[0]);
    close(h->selfpipe[1]);
    assert(!pthread_mutex_destroy(&h->post_mu));
}

static void config_isolation(const char *modules)
{
    char a[] = "/tmp/oa-config-a.XXXXXX", b[] = "/tmp/oa-config-b.XXXXXX";
    int fd = mkstemp(a);
    assert(fd >= 0);
    FILE *f = fdopen(fd, "w");
    assert(f);
    assert(fputs("# per-host snapshot\n ui.skin = vendor \n"
                 "sample = first\nsample = later\nempty=\n", f) >= 0);
    assert(!fclose(f));
    fd = mkstemp(b);
    assert(fd >= 0);
    f = fdopen(fd, "w");
    assert(f);
    /* Fill a separate host's complete table, then exceed its bound. */
    int cap = (int)(sizeof ((oa_host *)0)->config / sizeof ((oa_host *)0)->config[0]);
    for (int i = 0; i < cap + 2; i++)
        assert(fprintf(f, "key%d=value%d\n", i, i) > 0);
    assert(!fclose(f));
    oa_host *first = calloc(1, sizeof *first), *second = calloc(1, sizeof *second);
    assert(first && second);
    assert(!oa_host_init(first, modules, a));
    const char *borrowed = first->api.config_get(first, "sample", NULL);
    assert(borrowed && !strcmp(borrowed, "first"));
    assert(!strcmp(first->api.config_get(first, "empty", "fallback"), ""));
    assert(!strcmp(first->api.config_get(first, "ui.skin", "fallback"), "vendor"));
    assert(!oa_host_init(second, modules, b));
    assert(!second->api.config_get(second, "sample", NULL));
    assert(second->config_count == cap);
    char last_key[32], over_key[32], last_value[32];
    snprintf(last_key, sizeof last_key, "key%d", cap - 1);
    snprintf(over_key, sizeof over_key, "key%d", cap);
    snprintf(last_value, sizeof last_value, "value%d", cap - 1);
    assert(!strcmp(second->api.config_get(second, last_key, NULL), last_value));
    assert(!second->api.config_get(second, over_key, NULL));
    assert(!strcmp(borrowed, "first"));
    assert(!first->api.config_get(first, "key0", NULL));
    strcpy(first->skin_selected, "omarchy");
    assert(!strcmp(first->api.config_get(first, "ui.skin", NULL), "omarchy"));
    assert(!second->api.config_get(second, "ui.skin", NULL));
    /* Files are an init snapshot: neither file edits nor another host's init
     * may change a running host's borrowed configuration strings. */
    f = fopen(a, "w");
    assert(f);
    assert(fputs("sample=replacement\n", f) >= 0);
    assert(!fclose(f));
    assert(!strcmp(borrowed, "first"));
    close_host(first);
    assert(!oa_host_init(first, modules, a));
    assert(!strcmp(first->api.config_get(first, "sample", NULL), "replacement"));
    assert(!first->api.config_get(first, "ui.skin", NULL));
    assert(!first->api.config_get(first, "empty", NULL));
    close_host(first);
    assert(!oa_host_init(first, modules, NULL));
    assert(!first->api.config_get(first, "sample", NULL));
    assert(first->config_count == 0);
    close_host(first);
    assert(!unlink(a));
    assert(!oa_host_init(first, modules, a));
    assert(!first->api.config_get(first, "sample", NULL));
    assert(first->config_count == 0);
    assert(!strcmp(second->api.config_get(second, "key0", NULL), "value0"));
    close_host(first);
    close_host(second);
    free(first);
    free(second);
    assert(!unlink(b));
    puts("  ok   configuration isolates hosts, capacity and borrowed values; reinit clears stale settings");
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    config_isolation(argv[1]);
    oa_host h;
    int iface = 1;
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    h.cur_module = 1;
    assert(h.api.publish(&h, "capacity-test", 1, &iface) == 0);
    for (int i = 0; i < MAX_HOLDS; i++) {
        h.holds[i].used = 1;
        h.holds[i].holder = 999;
        strcpy(h.holds[i].name, "other-service");
    }
    h.cur_module = 2;
    assert(h.api.lookup(&h, "capacity-test", 1) == NULL);
    h.holds[0].used = 0;
    assert(h.api.lookup(&h, "capacity-test", 1) == &iface);
    /* A repeated lookup reuses its hold even when the table is otherwise full. */
    assert(h.api.lookup(&h, "capacity-test", 1) == &iface);
    h.api.release(&h, "capacity-test");
    assert(!h.holds[0].used);
    puts("  ok   full hold table refuses a new pointer; repeat lookup reuses its hold");

    oa_host q;
    assert(oa_host_init(&q, argv[1], NULL) == 0);
    oa_mod *m = oa_module_load(&q, "ticker");
    assert(m && oa_module_start(&q, m) == 0);
    q.cur_module = 999;
    assert(q.api.lookup(&q, "clock", 1));
    q.cur_module = 0;
    assert(oa_module_unload(&q, "ticker", 0) == -1);
    char command[] = "stop ticker", reply[256];
    assert(oa_control_command(&q, command, reply, sizeof reply) == -1);
    assert(strstr(reply, "failed"));
    assert(m->state == OA_ST_STARTED);
    assert(oa_host_service_exists(&q, "clock"));
    assert(oa_module_unload(&q, "ticker", 0) == -1);
    q.cur_module = 999;
    q.api.release(&q, "clock");
    q.cur_module = 0;
    assert(oa_module_stop(&q, m) == 0);
    assert(oa_module_unload(&q, "ticker", 0) == 0);
    puts("  ok   stopping a held provider is refused until the consumer releases it");

    assert(oa_module_load(&q, argv[2]) == NULL);
    assert(!oa_host_service_exists(&q, "failed-init"));
    for (int i = 0; i < MAX_TIMERS; i++) assert(!q.timers[i].active);
    for (int i = 0; i < MAX_SUBS; i++) assert(!q.subs[i].active);
    puts("  ok   failed init leaves no registered service or timer");
    close(h.selfpipe[0]); close(h.selfpipe[1]);
    close(q.selfpipe[0]); close(q.selfpipe[1]);
    return 0;
}
