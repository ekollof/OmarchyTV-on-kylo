/* Real control parser plus the real audio module. Hal_Audio_Ctrl is a stub in
 * the test DSO so the enabled path can refuse standby without a TV. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
#include "lifecycle_retry.h"
#include "openaurora/services.h"

static int command(oa_host *host, const char *text, char *out, size_t cap)
{
    char line[128];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(host, line, out, cap);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    oa_host host;
    char out[512];
    assert(!oa_host_init(&host, argv[1], argv[2]));
    host.log_level = OA_LOG_ERROR;
    assert(!command(&host, "load plat", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start plat", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "load audio", out, sizeof out) && strstr(out, "ok"));
    assert(!command(&host, "start audio", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!command(&host, "status audio", out, sizeof out));
    assert(strstr(out, "state   started") && strstr(out, "audio:v1"));
    const oa_audio_iface *service = host.api.lookup(&host, OA_SVC_AUDIO, 1);
    assert(service && service->bind_session);
    assert(service->bind_session(NULL) == -1 && errno == ENOTSUP);
    int sentinel = 1;
    assert(service->bind_session(&sentinel) == -1 && errno == ENOTSUP);
    host.api.release(&host, OA_SVC_AUDIO);
    assert(command(&host, "suspend", out, sizeof out) == -1 && !strcmp(out, "suspend failed\n"));
    oa_mod *audio = oa_host_find_module(&host, "audio");
    assert(audio && audio->state == OA_ST_STARTED);
    assert(oa_host_find_module(&host, "plat"));
    assert(!lifecycle_retry(&host, "unload audio", out, sizeof out) && !strcmp(out, "ok\n"));
    assert(!oa_host_find_module(&host, "audio") && oa_host_find_module(&host, "plat"));
    host.stop_requested = 1;
    assert(!oa_host_run(&host) && !oa_host_find_module(&host, "plat"));
    puts("  ok   suspend failed while the HAL is enabled; the module stayed started and the host stayed up");
    return 0;
}
