/* Regression coverage for the checked-off loader claims audited on 2026-10-31. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "host.h"

int main(int argc, char **argv)
{
    assert(argc == 3);
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
