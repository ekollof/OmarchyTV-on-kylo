/* The control batch queue must run one module operation per host-loop turn,
 * keep the remaining entries, and survive a failed/missing entry. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host.h"

static int command(oa_host *h, const char *text, const char *expected)
{
    char line[1024], reply[512];
    snprintf(line, sizeof line, "%s", text);
    int rc = oa_control_command(h, line, reply, sizeof reply);
    if (strcmp(reply, expected)) {
        fprintf(stderr, "command '%s' -> '%s', expected '%s'\n",
                text, reply, expected);
        assert(0);
    }
    return rc;
}

static int state(oa_host *h, const char *name)
{
    oa_mod *m = oa_host_find_module(h, name);
    return m ? m->state : OA_ST_EMPTY;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    oa_host h;
    assert(oa_host_init(&h, argv[1], NULL) == 0);
    h.log_level = -1; /* expected failures are checked, not printed */

    /* Nothing runs until the loop pumps; the reply never waits for modules. */
    command(&h, "load-batch batch_a batch_b batch_c", "queued 3\n");
    assert(state(&h, "batch_a") == OA_ST_EMPTY);
    assert(state(&h, "batch_b") == OA_ST_EMPTY);
    assert(state(&h, "batch_c") == OA_ST_EMPTY);
    assert(oa_control_pending(&h) == 1);

    oa_control_pump(&h);
    assert(state(&h, "batch_a") == OA_ST_LOADED);
    assert(state(&h, "batch_b") == OA_ST_EMPTY);
    assert(state(&h, "batch_c") == OA_ST_EMPTY);

    oa_control_pump(&h);
    assert(state(&h, "batch_b") == OA_ST_LOADED);
    assert(state(&h, "batch_c") == OA_ST_EMPTY);

    oa_control_pump(&h);
    assert(state(&h, "batch_c") == OA_ST_LOADED);
    assert(oa_control_pending(&h) == 0);
    puts("  ok   batch_load: one module per pump and nothing before the pump");

    /* start-batch loads what is missing and starts one module per turn. */
    command(&h, "start-batch batch_a batch_c", "queued 2\n");
    assert(state(&h, "batch_a") == OA_ST_LOADED);
    oa_control_pump(&h);
    assert(state(&h, "batch_a") == OA_ST_STARTED);
    assert(state(&h, "batch_c") == OA_ST_LOADED);
    oa_control_pump(&h);
    assert(state(&h, "batch_c") == OA_ST_STARTED);
    assert(oa_control_pending(&h) == 0);
    puts("  ok   batch_load: start-batch starts one module per pump");

    /* Missing entries are reported by the pump and do not stall later work. */
    command(&h, "load-batch no_such_module_xyz batch_a", "queued 2\n");
    oa_control_pump(&h);
    assert(state(&h, "batch_a") == OA_ST_STARTED);
    oa_control_pump(&h);
    assert(state(&h, "batch_a") == OA_ST_STARTED); /* load keeps a started module */
    assert(oa_control_pending(&h) == 0);
    puts("  ok   batch_load: missing entry does not stall the rest");

    /* A queue overflow is explicit, and every accepted entry still drains. */
    char overflow[1024] = "load-batch";
    for (int i = 0; i < 70; i++) {
        char name[32];
        snprintf(name, sizeof name, " missing_%02d", i);
        strncat(overflow, name, sizeof overflow - strlen(overflow) - 1);
    }
    command(&h, overflow, "queued 64 dropped 6\n");
    for (int i = 0; i < 64; i++) {
        assert(oa_control_pending(&h) == 1);
        oa_control_pump(&h);
    }
    assert(oa_control_pending(&h) == 0);
    assert(state(&h, "batch_a") == OA_ST_STARTED);
    puts("  ok   batch_load: bounded queue reports drops and drains fully");

    /* Oversized names are rejected before touching the queue. */
    char huge[256];
    memset(huge, 'x', sizeof huge - 1);
    huge[sizeof huge - 1] = 0;
    char line[512];
    snprintf(line, sizeof line, "load-batch %s batch_c", huge);
    command(&h, line, "queued 1 dropped 1\n");
    oa_control_pump(&h);
    assert(oa_control_pending(&h) == 0);
    puts("  ok   batch_load: oversized name is rejected");

    assert(command(&h, "load-batch", "usage: load-batch <name> [name...]\n") == -1);
    puts("  ok   batch_load: missing names give usage, not a partial run");
    return 0;
}
