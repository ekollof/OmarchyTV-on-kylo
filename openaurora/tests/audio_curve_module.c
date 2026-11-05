#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "host.h"

static const char *aq_path, *profile;
static const char *config(void *priv, const char *key, const char *def)
{
    (void)priv;
    if (!strcmp(key, "audio_curve.aq")) return aq_path;
    if (!strcmp(key, "audio_curve.profile")) return profile;
    return def;
}
static void write_aq(const char *path, unsigned base, size_t size)
{
    uint8_t bytes[OA_AUDIO_AQ_SIZE + 1] = {0};
    assert(size <= sizeof bytes);
    for (unsigned i = 0; i < OA_AUDIO_CURVE_POINTS; i++) {
        unsigned values[] = {base + i, base + 500 + i};
        size_t offsets[] = {0x24 + 4*i, 0x1b8 + 4*i};
        for (unsigned j = 0; j < 2; j++) {
            bytes[offsets[j]] = (uint8_t)values[j];
            bytes[offsets[j]+1] = (uint8_t)(values[j] >> 8);
        }
    }
    FILE *file = fopen(path, "wb"); assert(file);
    assert(fwrite(bytes, 1, size, file) == size && !fclose(file));
}
static const oa_audio_curve_iface *service(oa_host *h)
{
    const oa_audio_curve_iface *s = h->api.lookup(h, OA_SVC_AUDIO_CURVE, 1);
    assert(s); return s;
}
static void value(const oa_audio_curve_iface *s, unsigned volume, unsigned expected)
{
    uint16_t actual = 0;
    assert(!s->value(volume, &actual) && actual == expected);
}
static int command(oa_host *h, const char *text)
{
    char line[80], reply[256];
    snprintf(line, sizeof line, "%s", text);
    int rc = oa_control_command(h, line, reply, sizeof reply);
    assert(strstr(reply, rc ? "failed" : "ok")); return rc;
}
static int command_text(oa_host *h, const char *text, char *out, size_t cap)
{
    char line[80];
    snprintf(line, sizeof line, "%s", text);
    return oa_control_command(h, line, out, cap);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    char directory[] = "/tmp/oa-curve-module.XXXXXX";
    assert(mkdtemp(directory));
    char file[256], fifo[256];
    snprintf(file, sizeof file, "%s/AQ.bin", directory);
    snprintf(fifo, sizeof fifo, "%s/fifo", directory);
    assert(!mkfifo(fifo, 0600));
    write_aq(file, 100, OA_AUDIO_AQ_SIZE);
    oa_host *h = calloc(1, sizeof *h); assert(h);
    assert(!oa_host_init(h, argv[1], NULL));
    h->log_level = -1; h->api.config_get = config;
    aq_path = file; profile = "0";
    oa_mod *m = oa_module_load(h, "audio_curve"); assert(m);
    assert(!command(h, "start audio_curve"));
    const oa_audio_curve_iface *s = service(h);
    oa_audio_curve copy;
    assert(!s->copy(&copy) && copy.points[10] == 110);
    assert(s->copy(NULL) == -1 && s->value(0, NULL) == -1);
    value(s, UINT_MAX, 200);
    char reply[2048];
    assert(!command_text(h, "audio-curve 10", reply, sizeof reply) &&
        !strcmp(reply, "curve 110\n"));
    assert(!command_text(h, "audio-curve 101", reply, sizeof reply) &&
        !strcmp(reply, "curve 200\n"));
    assert(!command_text(h, "audio-curve-dump", reply, sizeof reply) &&
        !strncmp(reply, "curve 101 100 101 102", 21));
    write_aq(file, 200, OA_AUDIO_AQ_SIZE);
    value(s, 10, 110); /* Input-file edits never mutate a published snapshot. */
    uint32_t old = m->id;
    assert(!command(h, "reload audio_curve"));
    m = oa_host_find_module(h, "audio_curve"); assert(m && m->id != old);
    s = service(h); value(s, 10, 210);
    assert(!command_text(h, "audio-curve 10", reply, sizeof reply) &&
        !strcmp(reply, "curve 210\n"));
    assert(copy.points[10] == 110); /* Caller data survives DSO replacement. */
    puts("  ok   audio-curve 10/101 and audio-curve-dump return the published AQ table over the control command");
    puts("  ok   real DSO/CLI start and reload reread AQ; published snapshots and caller copies remain immutable");

    oa_mod *consumer = oa_module_load(h, "ticker"); assert(consumer);
    h->cur_module = consumer->id;
    assert(h->api.lookup(h, OA_SVC_AUDIO_CURVE, 1) == s);
    h->cur_module = 0;
    old = m->id;
    assert(command(h, "stop audio_curve") == -1);
    assert(command(h, "unload audio_curve") == -1);
    assert(command(h, "reload audio_curve") == -1);
    assert(m->id == old && m->state == OA_ST_STARTED); value(s, 10, 210);
    h->cur_module = consumer->id; h->api.release(h, OA_SVC_AUDIO_CURVE); h->cur_module = 0;
    assert(!oa_module_unload(h, "ticker", 0));
    assert(!command(h, "stop audio_curve") && !oa_host_service_exists(h, OA_SVC_AUDIO_CURVE));
    puts("  ok   real registry consumer hold blocks stop/unload/reload; release allows clean stop");

    int peer = 1;
    assert(!h->api.publish(h, OA_SVC_AUDIO_CURVE, 1, &peer));
    assert(command(h, "start audio_curve") == -1 && m->state == OA_ST_LOADED);
    assert(h->api.lookup(h, OA_SVC_AUDIO_CURVE, 1) == &peer);
    assert(!h->api.unpublish(h, OA_SVC_AUDIO_CURVE));
    assert(!command(h, "start audio_curve"));
    assert(!command(h, "stop audio_curve"));
    puts("  ok   publication collision preserves peer service; the same module can retry start");

    const char *invalid[] = {NULL, "", "1junk", "2147483648", "-2147483649", "99999999999999999999999"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        profile = invalid[i]; assert(oa_module_start(h, m) == -1);
        assert(m->state == OA_ST_LOADED && !oa_host_service_exists(h, OA_SVC_AUDIO_CURVE));
    }
    profile = "0";
    const char *bad_paths[] = {NULL, "", "/this-path-does-not-exist/AQ.bin", directory, fifo};
    for (size_t i = 0; i < sizeof bad_paths / sizeof bad_paths[0]; i++) {
        aq_path = bad_paths[i]; assert(oa_module_start(h, m) == -1);
        assert(!oa_host_service_exists(h, OA_SVC_AUDIO_CURVE));
    }
    aq_path = file;
    write_aq(file, 200, OA_AUDIO_AQ_SIZE - 1); assert(oa_module_start(h, m) == -1);
    write_aq(file, 200, OA_AUDIO_AQ_SIZE + 1); assert(oa_module_start(h, m) == -1);
    write_aq(file, 200, OA_AUDIO_AQ_SIZE);
    profile = "1";
    assert(!oa_module_start(h, m)); s = service(h); value(s, 10, 710);
    puts("  ok   missing/invalid profile, absent/short/long file, directory and FIFO reject without publication; aggressive retry works");

    assert(!oa_module_suspend_all(h) && m->state == OA_ST_SUSPENDED);
    write_aq(file, 300, OA_AUDIO_AQ_SIZE);
    value(s, 10, 710);
    assert(!oa_module_resume_all(h)); value(s, 10, 710);
    assert(!oa_module_suspend_all(h));
    assert(!command(h, "reload audio_curve"));
    m = oa_host_find_module(h, "audio_curve");
    assert(m && m->state == OA_ST_SUSPENDED && !oa_host_service_exists(h, OA_SVC_AUDIO_CURVE));
    profile = "bad";
    assert(oa_module_resume_all(h) == -1 && m->state == OA_ST_SUSPENDED);
    assert(!oa_host_service_exists(h, OA_SVC_AUDIO_CURVE));
    profile = "1";
    assert(!oa_module_resume_all(h)); s = service(h); value(s, 10, 810);
    assert(!command(h, "unload audio_curve"));
    puts("  ok   standby retains immutable data; suspended reload defers I/O/publication, failed activation retries on resume");

    profile = "0"; m = oa_module_load(h, "audio_curve"); assert(m && !oa_module_start(h, m));
    assert(!unlink(file));
    assert(command(h, "reload audio_curve") == -1);
    assert(!oa_host_find_module(h, "audio_curve") && !oa_host_service_exists(h, OA_SVC_AUDIO_CURVE));
    write_aq(file, 400, OA_AUDIO_AQ_SIZE);
    m = oa_module_load(h, "audio_curve"); assert(m && !oa_module_start(h, m));
    value(service(h), 10, 410);
    assert(!oa_module_unload(h, "audio_curve", 0));
    puts("  ok   failed replacement leaves no stale provider; explicit load recovers (host has no old-binary rollback)");
    assert(!unlink(file) && !unlink(fifo) && !rmdir(directory));
    close(h->selfpipe[0]); close(h->selfpipe[1]); pthread_mutex_destroy(&h->post_mu);
    free(h); return 0;
}
