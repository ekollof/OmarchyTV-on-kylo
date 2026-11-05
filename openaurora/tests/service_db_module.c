/* Service inventory through the reloadable service_db module and CLI. */
#include "host.h"
#include "openaurora/services.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *db_path;

static const char *config(void *p, const char *key, const char *def)
{
    (void)p;
    if (!strcmp(key, "service_db.path")) return db_path;
    return def;
}

static int command(oa_host *h, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(h, line, out, cap);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    db_path = argv[2];
    FILE *probe = fopen(db_path, "rb");
    if (!probe) {
        fprintf(stderr, "skipping: %s unavailable\n", db_path);
        return 77;
    }
    fclose(probe);

    oa_host *h = calloc(1, sizeof *h);
    assert(h && !oa_host_init(h, argv[1], NULL));
    h->log_level = -1;
    h->api.config_get = config;
    oa_mod *m = oa_module_load(h, "service_db");
    assert(m && !oa_module_start(h, m));

    char out[8192];
    assert(!command(h, "service-list", out, sizeof out));
    assert(!strcmp(out, "count 525\n"));
    assert(!command(h, "service-list 0", out, sizeof out));
    assert(strstr(out, "source=dvb"));
    assert(strstr(out, "name="));
    assert(command(h, "service-list 525", out, sizeof out) == -1);
    assert(strstr(out, "failed errno 22"));
    assert(command(h, "service-list -1", out, sizeof out) == -1);
    assert(strstr(out, "failed errno 22"));
    assert(command(h, "service-list nope", out, sizeof out) == -1);
    assert(strstr(out, "failed errno 22"));
    assert(command(h, "service-list 0 extra", out, sizeof out) == -1);
    assert(strstr(out, "failed errno 22"));

    assert(!command(h, "reload service_db", out, sizeof out));
    m = oa_host_find_module(h, "service_db");
    assert(m);
    assert(!command(h, "service-list", out, sizeof out));
    assert(!strcmp(out, "count 525\n"));
    assert(!command(h, "service-list 0", out, sizeof out));
    assert(strstr(out, "source=dvb"));

    assert(!command(h, "stop service_db", out, sizeof out));
    assert(command(h, "service-list", out, sizeof out) == -1);
    assert(strstr(out, "failed errno 19"));
    assert(!command(h, "unload service_db", out, sizeof out));
    puts("ok service_db module: 525 DVB/EASI/analog rows via reloadable service.list and CLI");
    free(h);
    return 0;
}
